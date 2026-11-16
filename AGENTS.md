# Project Guide for AI Assistants

## User Preferences & Workflow

- **Language:** Communicate in Turkish. (Web UI and code comments stay in English.)
- **Coding & Upload:** The user does not write code manually. Write the code, build it, and upload it to the ESP32 via PlatformIO (`pio run -t upload`).
- **Git / GitHub:** The user is learning Git/GitHub step by step. Guide them on commits, pushes, and best practices without doing everything silently.
- **Safety:** Always remind the user about 220V safety (220V wires go ONLY to the relay screw terminals `COM` and `NO`, never on the perfboard) and never connect external 5V while USB is plugged in.

## Hardware

- **MCU:** ESP32-C3 Super Mini (`esp32-c3-devkitm-1`, native USB CDC)
- **RTC:** DS1302 with backup battery (CLK: GPIO 5, DAT: GPIO 6, RST: GPIO 7, VCC: 3.3V)
- **Relay:** 1-channel 5V transistor-driven module (`JQC3F-05VDC-C`, `Q1` + `D1`, no optocoupler), connected to GPIO 3 (`PIN_RELAY`), powered from external 5V.
- **Button:** Connected between GPIO 4 and GND (`INPUT_PULLUP`).
- **Load:** LED panel with ~30-40V driver; relay switches the 220V AC side (`COM` + `NO`).

## Current Status (as of last session)

- **Done:**
  - PlatformIO project initialized in `C:\Projects\LED-Alarm-2` and pushed to GitHub (`main`).
  - v1 firmware written, compiled, and uploaded to ESP32-C3 (`COM3`).
  - Phone WiFi AP (`LED-Alarm`, `http://192.168.4.1`) and panel toggle tested using the on-board blue LED (GPIO 8).
- **Next Steps:**
  1. Remove the relay module from its old project and test if it triggers with 3.3V (`RELAY_ACTIVE_HIGH = true` in `include/config.h`).
  2. Wire DS1302, button, and relay on a breadboard and test all features (time sync, RTC persistence, button single/double press, 3-min auto-off, 5-min snooze).
  3. Solder low-voltage parts onto perfboard and enclose the 220V relay terminals safely.
