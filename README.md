# Door Access Control

A Raspberry Pi based realtime door access control prototype with magstripe authentication, risk-aware face verification, servo lock control, and event-driven C++ modules.

![Project Icon](docs/images/DAC.png)



## Overview

This project is a realtime door access control system developed on Raspberry Pi for ENG5220.  
It combines credential input, verification logic, hardware feedback, and modular C++ components into a responsive embedded prototype.

## Core Features

- Magstripe-based access input
- Risk-triggered face verification
- Servo-based lock actuation
- LED and buzzer feedback
- Event-driven C++ design on Linux

## System Workflow

```mermaid
flowchart LR
    A[Card Swipe] --> B[Magstripe Reader]
    B --> C{Card Valid?}
    C -->|No| D[Access Denied]
    C -->|Yes| E{Face Verification Required?}
    E -->|Yes| F{Face Match?}
    E -->|No| G[Access Granted]
    F -->|Yes| G
    F -->|No| D
    G --> H[Servo Lock / LEDs]
    D --> I[LEDs / Buzzer]
```
### System Status Indication

The current implementation provides system feedback through LEDs, buzzer, and servo lock control.

| System State | Trigger | LED Behaviour | Buzzer Behaviour | Lock Behaviour |
|---|---|---|---|---|
| Idle | System startup / reset | Yellow ON, Red OFF, Green OFF | Off | Locked |
| Access Denied | Invalid card or failed face verification | Red ON, Yellow ON, Green OFF | Three short beeps | Locked |
| Face Verification Required | Valid card under high-risk condition | Red ON, Yellow ON, Green ON | Two short beeps | Locked |
| Access Granted | Valid card accepted or face verification passed | Green ON, Yellow ON, Red OFF | One short beep | Unlocked temporarily, then locked again automatically |

> In the current code, **granted** and **denied** states are held for about **2 seconds** before returning to **Idle** automatically.

## Hardware

> Components were sourced from both China and the UK, so prices are shown in their original purchase currency.

| Image | Component | Purpose | Price |
|---|---|---|---|
| <img src="docs/images/pi5.jpg" width="120"> | Raspberry Pi 5 (4GB) | Main controller | Provided by lab |
| <img src="docs/images/breadboard.jpg" width="120"> | Breadboard | Circuit prototyping and wiring | ¥6 |
| <img src="docs/images/jumperwire.jpg" width="120"> | Jumper Wires (female-to-female, female-to-male) | GPIO and module connections | ¥8 |
| <img src="docs/images/magstripe.jpg" width="120"> | USB Magstripe Reader | Card input | ¥28 |
| <img src="docs/images/CAM.jpg" width="120"> | Camera | Face verification | ¥40 |
| <img src="docs/images/LED.jpg" width="120"> | LEDs (Red / Yellow / Green) | System status indication | ¥6 |
| <img src="docs/images/BUZZER.jpg" width="120"> | Buzzer | Alarm feedback | £4 |
| <img src="docs/images/SG90.jpg" width="120"> | SG90 Servo Motor | Door lock actuation | £6 |

## Prerequisites

- Linux on Raspberry Pi
- CMake 3.16+
- C++17 compiler
- OpenCV with `core`, `imgproc`, `highgui`, `videoio`, `objdetect`, and `face`
- `libgpiod`

Install the required packages:

```bash
sudo apt update
sudo apt install -y cmake g++ libgpiod-dev libopencv-dev libopencv-contrib-dev
```
## Build & Run

Build the project:

```bash
cmake -S . -B build
cmake --build build -j
```
Run the program:
```bash
./build/door_access_control
```

## Testing

The repository includes unit tests for selected modules.

```bash
cmake -S tests -B build/tests
cmake --build build/tests -j
ctest --test-dir build/tests --output-on-failure
```
Example test output:

![test Photo](docs/images/test.png)

## Social Media

Project updates and demo clips will be shared here:

- TikTok: https://www.tiktok.com/@d.a.control
- YouTube: https://www.youtube.com/channel/UC-V3Io8VhV6NMzuq4lF-zSw

## Team Contributions

- Guo Yinchen: core implementation, integration, hardware setup, documentation
- Zhuoxian Cai: to be updated
- Yin Bole: documentation support, unit test 
- Wenqiang Ding: initial logging prototype (not integrated into final system)
- Po Hsiang Chiu: to be updated

## License

This project is released under the MIT License.  
See the [LICENSE](LICENSE) file for details.
