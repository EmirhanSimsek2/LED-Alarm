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
  - PlatformIO project initialized in `C:\Projects\LED-Alarm` and pushed to GitHub (`main`).
  - Hardware wired and soldered onto perfboard; DS1302 RTC verified (`valid=1 running=1 wp=0`) and retains time across power loss.
  - Fixed ESP32-C3 USB CDC blocking delay (`Serial.setTxTimeoutMs(0)`) and web toggle race condition (`state:'toggle'`, non-overlapping `refresh()`).
  - v2 firmware compiled and uploaded to ESP32-C3 (`COM3`): configurable `alarmMin`, `snoozeMin`, `manualOffMin` (saved in `Preferences`), next-alarm display, `Copy Mon -> Fri`, `Disable all`, collapsible manual date/time sync, and `no-store` headers.
- **Next Steps:**
  1. Safely connect and enclose the 220V AC relay terminals (`COM` and `NO`) inside an insulated enclosure.
  2. Power from external 5V (with USB disconnected) and verify real-world LED panel switching.
