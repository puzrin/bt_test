#include "logger.hpp"
#include "blinker/blinker.h"
#include "button/button.hpp"
#include "rpc/rpc.hpp"
#include "async_preference/prefs.hpp"

void setup() {
    logger_init();
    prefs_init();
    blinker_init();
    button_init();

    // Demo methods
    rpc.addMethod("ping", []()-> std::string { return "pong"; });
    rpc.addMethod("sum", [](int32_t a, int32_t b)-> int32_t { return a + b; });
    rpc.addMethod("echo", [](const std::string msg)-> std::string { return msg; });
    rpc.addMethod("devnull", [](const std::string msg)-> bool { return true; });

    rpc_init();

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
}

void loop() {}