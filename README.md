# LED Alarm 2

ESP32-C3 Super Mini based wake-up light. An LED panel above the bed is switched by a relay at alarm time.

## Features
- Separate alarm time for each day of the week (saved in flash, survives power loss)
- DS1302 RTC keeps the time when power is off
- ESP32 creates its own WiFi network (`LED-Alarm`); the control page opens at http://192.168.4.1
- Phone's clock is synced to the ESP32 every time the page is opened
- Alarm turns itself off after 3 minutes
- Button: normal press toggles the panel. While ringing: 1 press = snooze 5 min, 2 presses = stop

## Wiring
| Part | Pin | ESP32-C3 |
|---|---|---|
| Relay module | IN / S | GPIO 3 |
| Relay module | VCC | 5V (external supply) |
| Button | one leg | GPIO 4 (other leg to GND) |
| DS1302 | CLK | GPIO 5 |
| DS1302 | DAT | GPIO 6 |
| DS1302 | RST | GPIO 7 |
| DS1302 | VCC | 3.3V |
| All GND | | GND |

> Disconnect the external 5V supply while the board is connected to the PC by USB.

## Build
1. Copy `include/secrets.example.h` to `include/secrets.h` and set the WiFi password.
2. Open in VS Code with PlatformIO and press **Upload**.
