#include "button.hpp"
#include "logger.hpp"

Button button;

void button_thread(void* pvParameters) {
    while (true) {
        button.tick(millis());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void button_init() {
    button.setEventHandler([](Button::Event event) {
        switch (event)
        {
        case Button::BUTTON_PRESSED_1X:
            DEBUG("Button pressed 1x");
            break;
        case Button::BUTTON_PRESSED_2X:
            DEBUG("Button pressed 2x");
            break;
        case Button::BUTTON_PRESSED_3X:
            DEBUG("Button pressed 3x");
            break;
        case Button::BUTTON_PRESSED_4X:
            DEBUG("Button pressed 4x");
            break;
        case Button::BUTTON_PRESSED_5X:
            DEBUG("Button pressed 5x");
            break;
        case Button::BUTTON_LONG_PRESS:
            DEBUG("Button long press");
            break;
        
        // Service events
        case Button::BUTTON_SEQUENCE_START:
            DEBUG("Button sequence start");
            break;
        case Button::BUTTON_SEQUENCE_END:
            DEBUG("Button sequence end");
            break;
        case Button::BUTTON_LONG_PRESS_START:
            DEBUG("Button long press start");
            break;
        default:
            DEBUG("Unknown button event: {}", int(event));
            break;
        }
    });
    xTaskCreate(button_thread, "button_thread", 1024 * 4, NULL, 1, NULL);
}
