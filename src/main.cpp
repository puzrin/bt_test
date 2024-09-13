#include "json_rpc_dispatcher.hpp"
#include "blinker.hpp"
#include "ble_manager.hpp"
#include "logger.hpp"

#include "blink.h"

BleManager bleManager("Reflow Table");

void setup() {
    logger_init();
    blink_init();

    blink.loop({
        { {255}, 300 },
        { 0, 300 },
        { 255, 300 },
        { 0, 300 },
        blink.flowTo({255}, 1500),
        blink.flowTo(0, 1500),
        blink.flowTo(255, 1500),
        blink.flowTo(0, 1500),
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