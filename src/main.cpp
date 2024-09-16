#include "ble_manager.hpp"
#include "logger.hpp"
#include "blinker/blinker.h"
#include "button/button.hpp"

BleManager bleManager("Reflow Table");

void setup() {
    logger_init();
    blinker_init();
    button_init();

    blinker.loop({
        { {255}, 300 },
        { 0, 300 },
        { 255, 300 },
        { 0, 300 },
        blinker.flowTo({255}, 1500),
        blinker.flowTo(0, 1500),
        blinker.flowTo(255, 1500),
        blinker.flowTo(0, 1500),
        { 0, 1000 }
    });

    // Demo methods
    bleManager.rpc.addMethod("ping", []()-> std::string { return "pong"; });
    bleManager.rpc.addMethod("sum", [](int32_t a, int32_t b)-> int32_t { return a + b; });
    bleManager.rpc.addMethod("echo", [](std::string msg)-> std::string { return msg; });
    bleManager.rpc.addMethod("devnull", [](std::string msg)-> bool { return true; });

    bleManager.start();
}

void loop() {}