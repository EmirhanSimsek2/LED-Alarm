#pragma once
#include <Arduino.h>

// ---------------- Pins (ESP32-C3 Super Mini) ----------------
// Avoid GPIO 2, 8, 9 (boot strapping) and 18, 19 (USB) for external parts.
constexpr uint8_t PIN_RELAY      = 3;   // Relay module IN / S
constexpr uint8_t PIN_BUTTON     = 4;   // Button to GND (internal pull-up)
constexpr uint8_t PIN_RTC_CLK    = 5;   // DS1302 CLK
constexpr uint8_t PIN_RTC_DAT    = 6;   // DS1302 DAT (I/O)
constexpr uint8_t PIN_RTC_RST    = 7;   // DS1302 RST (CE)
constexpr uint8_t PIN_STATUS_LED = 8;   // On-board blue LED (active LOW), mirrors panel state

// true  -> relay turns ON when pin is HIGH (most "S / + / -" modules)
// false -> relay turns ON when pin is LOW  ("low level trigger" modules)
constexpr bool RELAY_ACTIVE_HIGH = true;

// ---------------- Timing ----------------
constexpr uint32_t ALARM_DURATION_MS = 3UL * 60UL * 1000UL;  // auto-off after 3 min
constexpr uint32_t SNOOZE_MS         = 5UL * 60UL * 1000UL;  // snooze 5 min
constexpr uint32_t DOUBLE_PRESS_MS   = 500;                  // window for a double press
constexpr uint32_t DEBOUNCE_MS       = 30;
