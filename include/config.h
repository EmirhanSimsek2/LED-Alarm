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
constexpr uint16_t DEFAULT_ALARM_MIN      = 3;    // auto-off after 3 min (0 = manual off)
constexpr uint16_t DEFAULT_SNOOZE_MIN     = 5;    // snooze 5 min
constexpr uint16_t DEFAULT_MANUAL_OFF_MIN = 0;    // manual panel auto-off (0 = disabled)
constexpr uint16_t MAX_DURATION_MIN       = 180;  // max alarm / manual auto-off minutes
constexpr uint16_t MAX_SNOOZE_MIN         = 60;   // max snooze minutes
constexpr uint32_t DOUBLE_PRESS_MS        = 500;  // window for a double press
constexpr uint32_t DEBOUNCE_MS            = 30;

// ---------------- Alarms ----------------
constexpr uint8_t MAX_ALARMS      = 20;
constexpr uint8_t ALARM_LABEL_LEN = 24;
