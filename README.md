<div align="center">

# 🔧 ESP32 Handheld

**A battery-powered ESP32 handheld with an OLED, four buttons and magnetic charging, built around a 3D-printed enclosure designed from scratch in FreeCAD.**

![ESP32](https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![FreeCAD](https://img.shields.io/badge/FreeCAD-1.1-C0392B?style=for-the-badge&logo=freecad&logoColor=white)
![Material](https://img.shields.io/badge/print-Bambu%20Lab%20Basic%20PLA-444?style=for-the-badge)
![Status](https://img.shields.io/badge/firmware-alpha-orange?style=for-the-badge)
<br>
<p float="left">
<img width="1791" height="1264" alt="image" src="https://github.com/user-attachments/assets/146682ba-0769-4d34-8746-32416dc305ae" />
<img width="1792" height="1259" alt="image" src="https://github.com/user-attachments/assets/a28f19f8-ea44-4d2b-8ccf-d11ae8d0588b" />
</p>
</div>

## ✨ What it is

A small handheld for learning embedded C++. It started as a bike-computer prototype (menu, battery meter, speed from a reed switch) and grew into a complete hardware project: custom enclosure, drawings for every printed part, and firmware that runs on the board.

| | |
|---|---|
| **Enclosure size** | 55 × 112 × 16.3 mm |
| **Printed in** | Bambu Lab Basic PLA |
| **Design tool** | FreeCAD 1.1 (parametric parts, assembly and TechDraw drawings) |
| **Tolerances** | General tolerances ISO 2768-m |
| **Status** | Enclosure designed, drawings finished. Firmware is an early alpha. |

## 🧩 Hardware

| Item | Part | Size (mm) |
|---|---|---|
| 1 | SSD1306 0.96" OLED display | 25.5 × 26.2 × 3 |
| 2 | ESP32 board | 29 × 58 × 4.7 |
| 3 | TP4056 charging module | 17.2 × 29.2 × 4 |
| 4 | Magnetic pogo-pin connector (charging) | 15.3 × 4.5 × 4 |
| 5–8 | M3 screw holes (four corners) | |
| 9–12 | Printed tact switch caps (×4) | 14 × 10 × 5.9 |
| 13 | Tact switch (×4) | 12.6 × 12.6 × 7.2 |
| | Li-ion cell in the battery bay | <!-- TODO: cell type and capacity --> |

## 🛠️ The enclosure

The case is split into a bottom part and a top part, held together by four M3 screws. The top part has:

- a window for the OLED
- four button openings
- screw bosses in the corners

The buttons are separate printed caps that sit on the tact switches. Small side tabs hold each cap in its opening, so the buttons don't fall out and need no glue.

## 💻 Firmware (alpha)

The firmware is written in C++ with the Arduino framework. It is unfinished, and I'm working on it. What exists so far:

- an OLED menu with a cursor and sub-menus
- four buttons handled by interrupts (ReactESP), with debounce
- battery voltage measurement through a voltage divider, with a calibration screen
- a reed-switch input for wheel speed and a ride timer
- LittleFS and JSON for settings (in development)

> **Status:** the menu navigation is still being wired up. See [`/firmware`](firmware/).

### Pins

| Function | GPIO |
|---|---|
| Button Up / Down / Select / Back | 25 / 26 / 27 / 13 |
| Battery voltage (ADC, 100 kΩ / 100 kΩ divider) | 39 |
| Reed switch (needs an external pull-up) | 34 |
| OLED (I2C address 0x3C) | default I2C pins |

### Libraries

Adafruit GFX, Adafruit SSD1306, ReactESP, ArduinoJson and LittleFS. Board: ESP32 Dev Module.

## 🔎 Related projects

- [Spring-release key fob](https://github.com/dmytro-buniak/key-fob-spring-release): a mechanism designed from scratch, with assembly drawings
- [Plastic-holder-reverse-engineering](https://github.com/dmytro-buniak/plastic-holder-reverse-engineering): 3D printable part replacement
