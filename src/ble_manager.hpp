#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include "json_rpc_dispatcher.hpp"
#include "ble_chunker.hpp"
#include "ble_auth_store.hpp" 
#include "logger.hpp"

namespace blem_ns {

class Session {
public:
    Session(JsonRpcDispatcher& rpc)
        : chunker(500, 16*1024 + 500), rpc(rpc) {
        chunker.onMessage = [this, &rpc](const std::vector<uint8_t>& message) {
            size_t freeMemory = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            size_t minimumFreeMemory = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
            DEBUG("Free memory: {} Minimum free memory: {}", uint32_t(freeMemory), uint32_t(minimumFreeMemory));

            DEBUG("BLE: Received message of length {}", uint32_t(message.size()));

            std::vector<uint8_t> response;
            rpc.dispatch(message, response);
            return response;
        };
    }

    BleChunker chunker;

private:
    JsonRpcDispatcher& rpc;
};

using Sessions = std::map<uint16_t, Session*>;

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    CharacteristicCallbacks(blem_ns::Sessions& sessions) : sessions(sessions) {}

    void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        DEBUG("BLE: Received chunk of length {}", uint32_t(pCharacteristic->getDataLength()));
        sessions[conn_handle]->chunker.consumeChunk(
            pCharacteristic->getValue(), pCharacteristic->getDataLength());
    }

    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        uint16_t conn_handle = desc->conn_handle;
        if (!sessions.count(conn_handle)) return;
        //DEBUG("BLE: Reading from characteristic, conn_handle {}", conn_handle);
        pCharacteristic->setValue(sessions[conn_handle]->chunker.getResponseChunk());
    }

private:
    blem_ns::Sessions& sessions;
};

} // namespace blem_ns

class BleManager {
public:
    BleManager(const std::string& deviceName)
        : deviceName(deviceName), server(nullptr), service(nullptr), characteristic(nullptr) {}

    void start() {
        const std::string name = deviceName.substr(0, 20); // Limit name length
        NimBLEDevice::init(name);
        NimBLEDevice::setMTU(517); // Set the maximum MTU size the server will support
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Set the power level to maximum

        server = NimBLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks(*this));

        // Setup RPC service and characteristic.
        service = server->createService(SERVICE_UUID);
        characteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID,
            // This hangs comm if enabled. Seems Web BT not supports encryption
            //NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC |
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE            
        );

        characteristic->setCallbacks(new blem_ns::CharacteristicCallbacks(sessions));
        service->start();

        // Configure advertising
        NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        NimBLEDevice::startAdvertising();

        DEBUG("BLE initialized");
    }

    JsonRpcDispatcher rpc;
    BleAuthStore<> authStore;

private:
    std::string deviceName;
    NimBLEServer* server;
    NimBLEService* service;
    NimBLECharacteristic* characteristic;
    blem_ns::Sessions sessions;

    static const char* SERVICE_UUID;
    static const char* CHARACTERISTIC_UUID;

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(BleManager& manager) : manager(manager) {}

        void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
            using namespace blem_ns;

            uint16_t conn_handle = desc->conn_handle;
            manager.sessions[conn_handle] = new Session(manager.rpc);
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
            delete manager.sessions[conn_handle];
            manager.sessions.erase(conn_handle);
        }

        void onMTUChange(uint16_t mtu, ble_gap_conn_desc* desc) override {
            DEBUG("BLE: MTU updated to {}, conn_handle {}", mtu, desc->conn_handle);
        }

    private:
        BleManager& manager;
    };
};

// UUIDs for the BLE service and characteristic
const char* BleManager::SERVICE_UUID = "5f524546-4c4f-575f-5250-435f5356435f"; // _REFLOW_RPC_SVC_
const char* BleManager::CHARACTERISTIC_UUID = "5f524546-4c4f-575f-5250-435f494f5f5f"; // _REFLOW_RPC_IO__
