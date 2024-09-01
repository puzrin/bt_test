#include "json_rpc_dispatcher.hpp"
#include "blinker.hpp"
#include "ble_manager.hpp"
#include "logger.hpp"


Blinker blinker;

BleManager bleManager("Reflow Table");

void setup() {
    logger_init();

    blinker.loop({ 300, 300, 1000, 1000 });

    // Demo methods
    bleManager.rpc.addMethod("ping", []()-> std::string { return "pong"; });
    bleManager.rpc.addMethod("echo", [](std::string msg)-> std::string { return msg; });

    bleManager.start();
}

void loop() {}