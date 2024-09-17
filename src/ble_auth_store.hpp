#include <array>
#include <algorithm>
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>

using BleAuthId = std::array<uint8_t, 16>;
using BleAuthSecret = std::array<uint8_t, 32>;

template<size_t MaxRecords = 4>
class BleAuthStore {
public:
    BleAuthStore() : initialized(false), clients{}, timestamps{},
                     clientsVersion(0), lastAccepted_clientsVersion(0),
                     timestampsVersion(0), lastAccepted_timestampsVersion(0) {}

    struct Client {
        BleAuthId id{};
        BleAuthSecret secret{};
    };

    bool has(const BleAuthId& client_id) const {
        lazy_init();
        return std::any_of(clients.begin(), clients.end(), [&client_id](const Client& client) {
            return client.id == client_id;
        });
    }

    bool get_secret(const BleAuthId& client_id, BleAuthSecret& secret_out) const {
        lazy_init();
        auto it = std::find_if(clients.begin(), clients.end(), [&client_id](const Client& client) {
            return client.id == client_id;
        });
        if (it == clients.end()) return false;
        secret_out = it->secret;
        return true;
    }

    bool set_timestamp(const BleAuthId& client_id, uint32_t timestamp) {
        lazy_init();

        auto it = std::find_if(clients.begin(), clients.end(), [&client_id](const Client& client) {
            return client.id == client_id;
        });
        if (it == clients.end()) return false;

        auto index = std::distance(clients.begin(), it);
        uint32_t current_ts = timestamps[index];
        constexpr uint32_t one_day_ms = 24 * 60 * 60 * 1000;

        if (timestamp == 0 || timestamp > current_ts + one_day_ms || current_ts > timestamp) {
            // Mark transaction "in progress"
            timestampsVersion.fetch_add(1, std::memory_order_acquire);

            // Update timestamp
            timestamps[index] = timestamp;

            // Align bad timestamps if any
            for (size_t i = 0; i < MaxRecords; i++) {
                if (i != index && timestamps[i] != 0 && timestamps[i] > timestamp + one_day_ms) {
                    timestamps[i] = timestamp;
                }
            }

            // Commit new version
            timestampsVersion.fetch_add(1, std::memory_order_release);
        }

        return true;
    }

    bool create(const BleAuthId& client_id, const BleAuthSecret& secret) {
        lazy_init();
        auto it = std::find_if(clients.begin(), clients.end(), [&client_id](const Client& client) {
            return client.id == client_id;
        });

        if (it == clients.end()) {
            auto min_it = std::min_element(timestamps.begin(), timestamps.end());
            it = clients.begin() + std::distance(timestamps.begin(), min_it);
        }

        clientsVersion.fetch_add(1, std::memory_order_acquire);
        timestampsVersion.fetch_add(1, std::memory_order_acquire);

        it->id = client_id;
        it->secret = secret;
        timestamps[std::distance(clients.begin(), it)] = 0;

        clientsVersion.fetch_add(1, std::memory_order_release);
        timestampsVersion.fetch_add(1, std::memory_order_release);
        return true;
    }

private:
    bool initialized;
    std::array<Client, MaxRecords> clients;
    std::array<uint32_t, MaxRecords> timestamps;

    std::atomic<uint32_t> clientsVersion;
    uint32_t lastAccepted_clientsVersion;
    std::atomic<uint32_t> timestampsVersion;
    uint32_t lastAccepted_timestampsVersion;

    Preferences prefs;

    void writer_tick() {
        std::array<Client, MaxRecords> clients_copy;
        std::array<uint32_t, MaxRecords> timestamps_copy;

        const uint32_t clientsVersionBefore = clientsVersion.load(std::memory_order_acquire);
        const uint32_t timestampsVersionBefore = timestampsVersion.load(std::memory_order_acquire);

        const bool clients_updated = (lastAccepted_clientsVersion != clientsVersionBefore) &&
                                     (clientsVersionBefore % 2 == 0);
        const bool timestamps_updated = (lastAccepted_timestampsVersion != timestampsVersionBefore) &&
                                        (timestampsVersionBefore % 2 == 0);

        if (clients_updated) {
            clients_copy = clients;

            // Version should not change during data clone
            if (clientsVersionBefore == clientsVersion.load(std::memory_order_acquire)) {
                // Copy successful => save to storage
                prefs.putBytes("clients", clients.data(), sizeof(clients));
                lastAccepted_clientsVersion = clientsVersionBefore;
            }
        }

        if (timestamps_updated) {
            timestamps_copy = timestamps;

            // Version should not change during data clone
            if (timestampsVersionBefore == timestampsVersion.load(std::memory_order_acquire)) {
                // Copy successful => save to storage
                prefs.putBytes("timestamps", timestamps.data(), sizeof(timestamps));
                lastAccepted_timestampsVersion = timestampsVersionBefore;
            }
        }
    }

    void writer_thread(void* pvParameters) {
        auto* self = static_cast<BleAuthStore<MaxRecords>*>(pvParameters);

        while (true) {
            vTaskDelay(pdMS_TO_TICKS(500));
            self->writer_tick();
        }
    }

    void lazy_init() {
        if (!initialized) {
            initialized = true;

            prefs.begin("ble_auth", false);

            //
            // Load stored data to memory
            //

            size_t len = sizeof(clients);
            prefs.getBytes("clients", clients.data(), len);

            len = sizeof(timestamps);
            prefs.getBytes("timestamps", timestamps.data(), len);

            // Create writer
            xTaskCreate(writer_thread, "ble_auth_writer", 1024 * 4, this, 1, NULL);
        }
    }
};
