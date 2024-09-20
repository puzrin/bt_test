#include <Arduino.h>
#include "prefs.hpp"

AsyncPreferenceKV prefsKV;
AsyncPreferenceWriter prefsWriter(200, []() { return millis(); });

void prefs_init() {
}
