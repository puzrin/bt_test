#include "btn.hpp"
#include "logger.hpp"

Btn btn;

void btn_thread(void* pvParameters) {
    while (true) {
        btn.tick(millis());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void btn_init() {
    btn.setEventHandler([](Btn::Event event) {
        switch (event)
        {
        case Btn::BUTTON_PRESSED_1X:
            DEBUG("Button pressed 1x");
            break;
        case Btn::BUTTON_PRESSED_2X:
            DEBUG("Button pressed 2x");
            break;
        case Btn::BUTTON_PRESSED_3X:
            DEBUG("Button pressed 3x");
            break;
        case Btn::BUTTON_PRESSED_4X:
            DEBUG("Button pressed 4x");
            break;
        case Btn::BUTTON_PRESSED_5X:
            DEBUG("Button pressed 5x");
            break;
        case Btn::BUTTON_LONG_PRESS:
            DEBUG("Button long press");
            break;
        
        // Service events
        case Btn::BUTTON_SEQUENCE_START:
            DEBUG("Button sequence start");
            break;
        case Btn::BUTTON_SEQUENCE_END:
            DEBUG("Button sequence end");
            break;
        case Btn::BUTTON_LONG_PRESS_START:
            DEBUG("Button long press start");
            break;
        default:
            DEBUG("Unknown button event: {}", int(event));
            break;
        }
    });
    xTaskCreate(btn_thread, "btn_thread", 1024 * 4, NULL, 1, NULL);
}
