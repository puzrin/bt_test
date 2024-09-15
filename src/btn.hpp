#pragma once

#include <Arduino.h>
#include "button.hpp"

class BtnDriver : public IButtonDriver {
public:
    BtnDriver() : initialized(false) {};
    bool get() override {
        if (!initialized) {
            initialized = true;
            pinMode(btnPin, INPUT_PULLUP);
        }
        return digitalRead(btnPin) == LOW;
    }
private:
    static constexpr uint8_t btnPin = 9;
    bool initialized;
};

using Btn = Button<BtnDriver>;

extern Btn btn;
void btn_init();
