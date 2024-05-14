/*#include <Arduino.h>

#define LED_BUILTIN 8

void blinkTask(void *pvParameters) {
    int counter = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        digitalWrite(LED_BUILTIN, HIGH);
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, LOW);
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);

        Serial.print("Counter: ");
        Serial.println(counter++);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Output test");
    Serial.println(portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(blinkTask, "Blink Task", 2048, NULL, 1, NULL, 0);
}

void loop() {}
*/