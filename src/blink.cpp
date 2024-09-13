#include "blink.h"

Blink blink;

void blink_thread(void* pvParameters) {
    while (true) {
        blink.tick(20);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void blink_init() {
    xTaskCreate(blink_thread, "blink_thread", 1024 * 4, NULL, 1, NULL);
}