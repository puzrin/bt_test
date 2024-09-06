#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include "json_rpc_dispatcher.hpp"
#include "ble_chunker.hpp"
#include "logger.hpp"

class BleManager {
public:
    BleManager(const std::string& deviceName)
        : deviceName(deviceName), server(nullptr), service(nullptr), characteristic(nullptr) {}

    void start() {
        const std::string name = deviceName.substr(0, 20); // Limit name length
        BLEDevice::init(name);
        BLEDevice::setMTU(517); // Set the maximum MTU size the server will support

        // Setup services

        server = BLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks(this));

        service = server->createService(SERVICE_UUID);
        characteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE
        );
        characteristic->addDescriptor(new BLE2902());
        characteristic->setCallbacks(new CharacteristicCallbacks(this));

        service->start();

        // Configure advertising

        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        
        BLEDevice::startAdvertising();
        DEBUG("BLE intialized");
    }

    JsonRpcDispatcher rpc;

private:
    class Session {
    public:
        Session(BleManager* manager)
            : manager(manager), chunker() {
            chunker.onMessage = [this, manager](const BleMessage& message) {
                std::string request(message.begin(), message.end());
                DEBUG("RPC request: {}", request.c_str());
                std::string response = manager->rpc.dispatch(request);
                //DEBUG("RPC response: {}", response.c_str());
                BleMessage responseMessage(response.begin(), response.end());
                DEBUG("BLE: Received message of length {}", uint32_t(message.size()));
                return responseMessage;
            };
        }

        void consumeChunk(BLECharacteristic* pCharacteristic) {
            std::string value = pCharacteristic->getValue();
            BleChunk chunk(value.begin(), value.end());
            DEBUG("BLE: Received chunk of length {}", uint32_t(chunk.size()));
            chunker.consumeChunk(chunk);
        }

        void sendData(BLECharacteristic* pCharacteristic) {

            if (chunker.response.empty()) {
                static BleChunk noData{0};
                pCharacteristic->setValue(noData.data(), uint32_t(noData.size()));
                DEBUG("BLE: No data to send, sending empty chunk");
                return;
            }

            BleChunk chunk = chunker.response.front();
            chunker.response.erase(chunker.response.begin());
            DEBUG("BLE: Sending chunk of length {}", uint32_t(chunk.size()));
            pCharacteristic->setValue(chunk.data(), uint32_t(chunk.size()));
        }

    private:
        BleManager* manager;
        BleChunker chunker;
    };

    std::string deviceName;
    BLEServer* server;
    BLEService* service;
    BLECharacteristic* characteristic;
    std::map<uint16_t, Session*> sessions;

    static const char* SERVICE_UUID;
    static const char* CHARACTERISTIC_UUID;

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks(BleManager* manager) : manager(manager) {}

        void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->connect.conn_id;
            manager->sessions[connId] = new Session(manager);
            DEBUG("BLE: Device connected with connection ID {}", connId);
            // Update connection parameters for maximum performance and stability
            // min conn interval 7.5ms, max conn interval 15ms, latency 0, timeout 2s
            pServer->updateConnParams(param->connect.remote_bda, 0x06, 0x12, 0, 200);
        }

        void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->disconnect.conn_id;
            DEBUG("BLE: Device disconnected with connection ID {}", connId);
            delete manager->sessions[connId];
            manager->sessions.erase(connId);
        }

    private:
        BleManager* manager;
    };

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        CharacteristicCallbacks(BleManager* manager) : manager(manager) {}

        void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->write.conn_id;
            if (!manager->sessions.count(connId)) return;
            DEBUG("BLE: Writing to characteristic with connection ID {}", connId);
            manager->sessions[connId]->consumeChunk(pCharacteristic);
        }

        void onRead(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->read.conn_id;
            if (!manager->sessions.count(connId)) return;
            DEBUG("BLE: Reading from characteristic with connection ID {}", connId);
            manager->sessions[connId]->sendData(pCharacteristic);
        }

    private:
        BleManager* manager;
    };
};

// UUIDs for the BLE service and characteristic
const char* BleManager::SERVICE_UUID = "5f524546-4c4f-575f-5250-435f5356435f"; // _REFLOW_RPC_SVC_
const char* BleManager::CHARACTERISTIC_UUID = "5f524546-4c4f-575f-5250-435f494f5f5f"; // _REFLOW_RPC_IO__
