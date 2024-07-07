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

class BleManager {
public:
    BleManager(const std::string& deviceName)
        : deviceName(deviceName), server(nullptr), service(nullptr), characteristic(nullptr) {}

    void start() {
        BLEDevice::init(deviceName);
        BLEDevice::setMTU(517); // Set the maximum MTU size the server will support
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
        BLEDevice::startAdvertising();
    }

    JsonRpcDispatcher rpc;

private:
    class Session {
    public:
        Session(BleManager* manager)
            : manager(manager), chunker() {
            chunker.onMessage = [this, manager](const BleMessage& message) {
                std::string request(message.begin(), message.end());
                std::string response = manager->rpc.dispatch(request);
                BleMessage responseMessage(response.begin(), response.end());
                return responseMessage;
            };
            chunker.onRespond = [this](const std::vector<BleChunk>& chunks) {
                for (const auto& chunk : chunks) {
                    chunkQueue.push(chunk);
                }
            };
        }

        void consumeChunk(BLECharacteristic* pCharacteristic) {
            // Clear the queue for new RPC requests
            while (!chunkQueue.empty()) {
                chunkQueue.pop();
            }
            std::string value = pCharacteristic->getValue();
            BleChunk chunk(value.begin(), value.end());
            chunker.consumeChunk(chunk);
        }

        void sendData(BLECharacteristic* pCharacteristic) {
            if (chunkQueue.empty()) {
                static BleChunk noData{0};
                pCharacteristic->setValue(noData.data(), noData.size());
                return;
            }

            BleChunk chunk = chunkQueue.front();
            chunkQueue.pop();
            pCharacteristic->setValue(chunk.data(), chunk.size());
        }

    private:
        BleManager* manager;
        BleChunker chunker;
        std::queue<BleChunk> chunkQueue;
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
            // Update connection parameters for maximum performance and stability
            // min conn interval 7.5ms, max conn interval 15ms, latency 0, timeout 2s
            pServer->updateConnParams(param->connect.remote_bda, 0x06, 0x12, 0, 200);
        }

        void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->disconnect.conn_id;
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
            manager->sessions[connId]->consumeChunk(pCharacteristic);
        }

        void onRead(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t *param) override {
            uint16_t connId = param->read.conn_id;
            if (!manager->sessions.count(connId)) return;
            manager->sessions[connId]->sendData(pCharacteristic);
        }

    private:
        BleManager* manager;
    };
};

// UUIDs for the BLE service and characteristic
const char* BleManager::SERVICE_UUID = "5f524546-4c4f-575f-5250-435f5356435f"; // _REFLOW_RPC_SVC_
const char* BleManager::CHARACTERISTIC_UUID = "5f524546-4c4f-575f-5250-435f494f5f5f"; // _REFLOW_RPC_IO__
