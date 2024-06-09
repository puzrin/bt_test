#include <iostream>
#include <string>
#include "json_rpc_dispatcher.hpp"
#include "blinker.hpp"

Blinker blinker;

void setup() {
    blinker.loop({ 300, 300, 1000, 1000 });

    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("===== Output test");
}

void loop() {}