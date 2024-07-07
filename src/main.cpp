#include "json_rpc_dispatcher.hpp"
#include "blinker.hpp"
#include "ble_manager.hpp"

Blinker blinker;

BleManager bleManager("Reflow Table");

void setup() {
    blinker.loop({ 300, 300, 1000, 1000 });

    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("===== Output test");

    // Demo methods
    bleManager.rpc.addMethod("ping", []()-> std::string { return "pong"; });
    bleManager.rpc.addMethod("echo", [](std::string msg)-> std::string { return msg; });

    bleManager.start();
}

void loop() {}