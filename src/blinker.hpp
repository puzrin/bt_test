#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <array>

// Non blocking blinker to make series of flashes
// Time is in ms
//
// Create instance for LED on pin 13 and reversed control (0 - on, 1 - off):
//
// Blinker blinker(13, true);
//
// Somewhere in the code:
//
// 1s on, 1s off, 2s on, 2s off, ... and repeat infinitely.
//
// blinker.loop({ 1000, 1000, 2000, 2000 })

class Blinker {
private:
    int ledPin;
    bool reverse_control;
    QueueHandle_t sequenceQueue;
    TaskHandle_t blinkerTaskHandle;

    struct Sequence {
        std::array<uint16_t, 20> intervals;
        size_t length;
        bool looping;
    };

    static void blinkerTask(void* pvParameters) {
        Blinker* blinker = static_cast<Blinker*>(pvParameters);
        Sequence sequence;
        size_t currentStep = 0;
        bool hasJob = false;

        while (true) {
            if (xQueueReceive(blinker->sequenceQueue, &sequence, 0) == pdPASS) {
                currentStep = 0;
                hasJob = true;
                blinker->setLed(true); // Start with LED ON
            }

            if (hasJob) {
                blinker->setLed(currentStep % 2 == 0);
                vTaskDelay(pdMS_TO_TICKS(sequence.intervals[currentStep]));
                currentStep = (currentStep + 1) % sequence.length;

                if (!sequence.looping && currentStep == 0) {
                    blinker->setLed(false); // Ensure LED is turned off
                    hasJob = false;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
    }

    void setLed(bool state) {
        digitalWrite(ledPin, reverse_control ? (state ? LOW : HIGH) : (state ? HIGH : LOW));
    }

    void initBlinker() {
        pinMode(ledPin, OUTPUT);
        setLed(false); // Ensure LED is initially OFF
        sequenceQueue = xQueueCreate(1, sizeof(Sequence));
        xTaskCreate(blinkerTask, "Blinker Task", 1024, this, 1, &blinkerTaskHandle);
    }

public:
    // Use BLINKER_LED_PIN and BLINKER_LED_REVERSE from build environment
    Blinker() {
        #if !defined(BLINKER_LED_PIN)
            #error "BLINKER_LED_PIN is not defined and no pin number passed to the constructor."
        #endif

        ledPin = BLINKER_LED_PIN;

        #if defined(BLINKER_LED_REVERSE) && BLINKER_LED_REVERSE != 0
        reverse_control = true;
        #else
        reverse_control = false;
        #endif

        initBlinker();
    }

    // Example: Blinker blinker(13); - create instance for LED on pin 13
    Blinker(int pin) : ledPin(pin), reverse_control(false) {
        initBlinker();
    }

    // Example: Blinker blinker(13, true); - create instance for LED on pin 13 with reverse control
    Blinker(int pin, bool reverse) : ledPin(pin), reverse_control(reverse) {
        initBlinker();
    }

    // Execute LED on-off-on-off-... sequence and repeat infinitely (until user defines next), time in ms
    // Example: blinker.loop({200, 200, 300, 300, 1000, 1000});
    template<typename T>
    void loop(std::initializer_list<T> intervals) {
        static_assert(std::is_same<T, int>::value, "Intervals must be of type int");
        Sequence sequence;
        std::copy(intervals.begin(), intervals.end(), sequence.intervals.begin());
        sequence.length = intervals.size();
        sequence.looping = true;
        xQueueOverwrite(sequenceQueue, &sequence);
    }

    // Execute LED on-off-on-off-... sequence and stop at the end, time in ms
    // Example: blinker.once({200, 200, 1000, 1000, 2000});
    template<typename T>
    void once(std::initializer_list<T> intervals) {
        static_assert(std::is_same<T, int>::value, "Intervals must be of type int");
        Sequence sequence;
        std::copy(intervals.begin(), intervals.end(), sequence.intervals.begin());
        sequence.length = intervals.size();
        sequence.looping = false;
        xQueueOverwrite(sequenceQueue, &sequence);
    }

    // Turn the LED on
    void on() {
        loop({ 20 });
    }

    // Turn the LED off
    void off() {
        once({ 1 });
    }
};
