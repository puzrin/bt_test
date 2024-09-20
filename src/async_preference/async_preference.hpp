#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <vector>
#include <type_traits>

// Interface for key-value storage
class IAsyncPreferenceKV {
public:
    virtual void write(const std::string& ns, const std::string& key, uint8_t* buffer, size_t length) = 0;
    virtual void read(const std::string& ns, const std::string& key, uint8_t* buffer, size_t length) = 0;
    virtual size_t length(const std::string& ns, const std::string& key) = 0;
};

namespace async_preference_ns {

// Serializer for trivially copyable types
template <typename T>
struct TrivialSerializer {
    static void save(IAsyncPreferenceKV& kv, const std::string& ns, const std::string& key, const T& value) {
        kv.write(ns, key, reinterpret_cast<uint8_t*>(const_cast<T*>(&value)), sizeof(T));
    }

    static void load(IAsyncPreferenceKV& kv, const std::string& ns, const std::string& key, T& value) {
        const size_t size = kv.length(ns, key);

        if (size == 0) return; // Key not exists => nothing to load
        if (size != sizeof(T)) return; // Wrong size => broken data, ignore it

        kv.read(ns, key, reinterpret_cast<uint8_t*>(&value), sizeof(T));
    }
};

// Serializer for buffer-like types (with data(), size(), and resize())
// Primary goal is to fit std::string and std::vector
template <typename T>
struct BufferSerializer {
    static void save(IAsyncPreferenceKV& kv, const std::string& ns, const std::string& key, const T& value) {
        kv.write(ns, key, reinterpret_cast<uint8_t*>(const_cast<typename T::value_type*>(value.data())), value.size() * sizeof(typename T::value_type));
    }

    static void load(IAsyncPreferenceKV& kv, const std::string& ns, const std::string& key, T& value) {
        size_t size = kv.length(ns, key);

        if (size == 0) return; // Key not exists => nothing to load
        if (size % sizeof(typename T::value_type) != 0) return; // Wrong size => broken data, ignore it

        value.resize(size / sizeof(typename T::value_type));
        kv.read(ns, key, reinterpret_cast<uint8_t*>(value.data()), size);
    }
};

// Check if the type has data(), size(), and resize() methods
template <typename T, typename = void>
struct HasBufferTraits : std::false_type {};

template <typename T>
struct HasBufferTraits<T, std::void_t<decltype(std::declval<T>().data()),
                                      decltype(std::declval<T>().size()),
                                      decltype(std::declval<T>().resize(0))>> : std::true_type {};

// Helper template to trigger static_assert with a dependent type
template <typename T>
struct dependent_false : std::false_type {};

}

// Internal interface for AsyncPreference
class AsyncPreferenceTickable {
public:
    virtual void tick() = 0;
};

// Writer for asynchronous preferences
class AsyncPreferenceWriter {
public:
    AsyncPreferenceWriter(uint32_t ms_period = 200, uint32_t (*get_time)() = nullptr)
        : ms_period(ms_period), get_time(get_time), prev_run_ts(0) {}

    void add(AsyncPreferenceTickable& pref) { preferences.push_back(&pref); }

    void tick() {
        if (get_time) {
            uint32_t timestamp = get_time();
            if (timestamp < prev_run_ts) prev_run_ts = timestamp; // Handle overflow
            if (timestamp - prev_run_ts < ms_period) return;
            prev_run_ts = timestamp;
        }

        for (auto& pref : preferences) pref->tick();
    }

private:
    uint32_t ms_period;
    uint32_t (*get_time)();
    uint32_t prev_run_ts;
    std::vector<AsyncPreferenceTickable*> preferences;
};

template <typename T, typename Serializer = void>
class AsyncPreference : public AsyncPreferenceTickable {
public:
    AsyncPreference(IAsyncPreferenceKV& kv, const std::string& ns, const std::string& key, T initial = T()) :
        value{initial}, snapshot{}, version(0), lastAccepted_version(0), has_snapshot(false),
        kv{kv}, ns(ns), key(key), is_preloaded(false) {}

    T& get() {
        preload();
        return value;
    }

    void set(const T& value) {
        valueUpdateBegin();
        this->value = value;
        valueUpdateEnd();
    }

    // Those are public for case, when user wish to modify complex object
    // internals instead of .set() method. Not recommended for direct use.
    void valueUpdateBegin() {
        // This should not usually happen, because user call .get() at the start
        // to restore persistance. But if write is called first, we should
        // disable persistance restore.
        if (!is_preloaded) is_preloaded = true;

        version.fetch_add(1, std::memory_order_acquire);
    }
    void valueUpdateEnd() { version.fetch_add(1, std::memory_order_release); }

    //
    // Those are for calling by AsyncPreferenceWriter from another thread,
    // to avoid freezes.
    //
    void tick() override {
        makeSnapshot();
        saveSnapshot();
    }

    void makeSnapshot() {
        const uint32_t versionBefore = version.load(std::memory_order_acquire);

        // If version is odd, it means that value is being updated right now.
        if (lastAccepted_version != versionBefore && versionBefore % 2 == 0) {
            snapshot = value;
            // If version is still the same => snapshot is useable.
            if (versionBefore == version.load(std::memory_order_acquire)) {
                lastAccepted_version = versionBefore;
                has_snapshot = true;
            }
        }
    }

    void saveSnapshot() {
        using namespace async_preference_ns;

        if (!has_snapshot) return;

        if constexpr (!std::is_void_v<Serializer>) {
            // If custom serializer is provided, use it
            Serializer::save(kv, ns, key, snapshot);
        } else if constexpr (HasBufferTraits<T>::value) {
            // Use BufferSerializer if type has buffer-like traits
            BufferSerializer<T>::save(kv, ns, key, snapshot);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            // Use TrivialSerializer for trivially copyable types
            TrivialSerializer<T>::save(kv, ns, key, snapshot);
        } else {
            static_assert(dependent_false<T>::value, "No suitable serializer found for this type");
        }

        has_snapshot = false;
    }

private:
    T value;
    T snapshot;
    std::atomic<uint32_t> version;
    uint32_t lastAccepted_version;
    bool has_snapshot;
    IAsyncPreferenceKV& kv;
    std::string ns;
    std::string key;
    bool is_preloaded;

    // Fetch value from storage, if key exists. This is called only once in
    // life cycle. The next reads are always from memory only.
    void preload() {
        using namespace async_preference_ns;

        if (is_preloaded) return;

        if (kv.length(ns, key) == 0) return; // If key does not exist

        if constexpr (!std::is_void_v<Serializer>) {
            // Use custom serializer if provided
            Serializer::load(kv, ns, key, value);
        } else if constexpr (HasBufferTraits<T>::value) {
            // Use BufferSerializer if type has buffer-like traits
            BufferSerializer<T>::load(kv, ns, key, value);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            // Use TrivialSerializer for trivially copyable types
            TrivialSerializer<T>::load(kv, ns, key, value);
        } else {
            static_assert(dependent_false<T>::value, "No suitable serializer found for this type");
        }

        is_preloaded = true;
    }
};
