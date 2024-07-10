#include <Arduino.h>
#include "logger.hpp"

Logger logger;

static void LogOutputTask(void* pvParameters) {
    Serial.begin(115200);

    while (!Serial) vTaskDelay(pdMS_TO_TICKS(10));

    DEBUG("===== starting logger thread");

    while (true) {
        // TODO: Read log messages and print them to the serial port
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void logger_init() {
    xTaskCreate(LogOutputTask, "LogOutputTask", 4096, NULL, 1, NULL);
}