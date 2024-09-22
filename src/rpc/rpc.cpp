#include <Arduino.h>
#include <NimBLEDevice.h>
#include <map>
#include "logger.hpp"
#include "rpc.hpp"
#include "ble_chunker.hpp"
#include "async_preference/prefs.hpp"
#include "ble_auth_store.hpp"

JsonRpcDispatcher rpc;
JsonRpcDispatcher auth_rpc;

namespace {

// UUIDs for the BLE service and characteristic
const char* SERVICE_UUID = "5f524546-4c4f-575f-5250-435f5356435f"; // _REFLOW_RPC_SVC_
const char* RPC_CHARACTERISTIC_UUID = "5f524546-4c4f-575f-5250-435f494f5f5f"; // _REFLOW_RPC_IO__
const char* AUTH_CHARACTERISTIC_UUID = "5f524546-4c4f-575f-5250-435f41555448"; // _REFLOW_RPC_AUTH

auto bleAuthStore = BleAuthStore<4>(prefsKV);
auto bleNameStore = AsyncPreference<std::string>(prefsKV, "settings", "ble_name", "Reflow Table");

class Session {
public:
    Session()
        : rpcChunker(500, 16*1024 + 500), authChunker(500, 1*1024) {
        rpcChunker.onMessage = [](const std::vector<uint8_t>& message) {
            size_t freeMemory = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            size_t minimumFreeMemory = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
            DEBUG("Free memory: {} Minimum free memory: {}", uint32_t(freeMemory), uint32_t(minimumFreeMemory));

            DEBUG("BLE: Received message of length {}", uint32_t(message.size()));

            std::vector<uint8_t> response;
            rpc.dispatch(message, response);
            return response;
        };

        authChunker.onMessage = [](const std::vector<uint8_t>& message) {
            std::vector<uint8_t> response;
            auth_rpc.dispatch(message, response);
            return response;
        };
    }

    BleChunker rpcChunker;
    BleChunker authChunker;
};

std::map<uint16_t, Session*> sessions;

class RpcCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        DEBUG("BLE: Received chunk of length {}", uint32_t(pCharacteristic->getDataLength()));
        sessions[conn_handle]->rpcChunker.consumeChunk(
            pCharacteristic->getValue(), pCharacteristic->getDataLength());
    }

    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        //DEBUG("BLE: Reading from characteristic, conn_handle {}", conn_handle);
        pCharacteristic->setValue(sessions[conn_handle]->rpcChunker.getResponseChunk());
    }
};

class AuthCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        DEBUG("BLE AUTH: Received chunk of length {}", uint32_t(pCharacteristic->getDataLength()));
        sessions[conn_handle]->authChunker.consumeChunk(
            pCharacteristic->getValue(), pCharacteristic->getDataLength());
    }

    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        pCharacteristic->setValue(sessions[conn_handle]->authChunker.getResponseChunk());
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        sessions[conn_handle] = new Session();
        DEBUG("BLE: Device connected, conn_handle {}, encryption: {}", conn_handle, uint8_t(desc->sec_state.encrypted));

        // Set the maximum data length the server will support
        // (!) Seems not actual, effects not visible
        pServer->setDataLen(conn_handle, 251);

        // Update connection parameters for maximum performance and stability
        // min conn interval 7.5ms, max conn interval 7.5ms, latency 0, timeout 2s
        pServer->updateConnParams(conn_handle, 0x06, 0x06, 0, 200);
    }

    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        DEBUG("BLE: Device disconnected, conn_handle {}", conn_handle);
        delete sessions[conn_handle];
        sessions.erase(conn_handle);
    }

    void onMTUChange(uint16_t mtu, ble_gap_conn_desc* desc) override {
        DEBUG("BLE: MTU updated to {}, conn_handle {}", mtu, desc->conn_handle);
    }
};


void ble_init() {
    prefsWriter.add(bleAuthStore);
    prefsWriter.add(bleNameStore);

    const std::string name = bleNameStore.get().substr(0, 20); // Limit name length
    NimBLEDevice::init(name);
    NimBLEDevice::setMTU(517); // Set the maximum MTU size the server will support
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Set the power level to maximum

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    // Setup RPC service and characteristic.
    NimBLEService* service = server->createService(SERVICE_UUID);

    NimBLECharacteristic* rpc_characteristic = service->createCharacteristic(
        RPC_CHARACTERISTIC_UUID,
        // This hangs comm if enabled. Seems Web BT not supports encryption
        //NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC |
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE            
    );
    rpc_characteristic->setCallbacks(new RpcCharacteristicCallbacks());

    NimBLECharacteristic* auth_characteristic = service->createCharacteristic(
        AUTH_CHARACTERISTIC_UUID,
        // This hangs comm if enabled. Seems Web BT not supports encryption
        //NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC |
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE            
    );
    auth_characteristic->setCallbacks(new AuthCharacteristicCallbacks());

    service->start();

    // Configure advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    NimBLEDevice::startAdvertising();

    DEBUG("BLE initialized");
}

}

void rpc_init() {
    ble_init();
}
