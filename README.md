# Realtime Door Access Control System on Raspberry Pi

A Raspberry Pi based realtime door access control prototype using **magstripe authentication**, **face verification**, and **relay-based door unlocking**.  
The system is implemented in **C++** with an **event-driven, modular, object-oriented** design on Linux.

---

## 1. Project Overview

This project is a prototype door access control system designed for a small secure area.  
It combines hardware and software modules on a Raspberry Pi to provide:

- credential input through a **magstripe reader**
- additional **face verification**
- realtime status feedback through **LED indicators**
- door unlocking through a **relay module**

---

## 2. Final Scope

### Core features
- Magstripe card input
- Allowlist-based credential verification
- Face verification
- LED status indication (`idle`, `granted`, `denied`)
- Relay-controlled door unlocking
- shutdown on `Ctrl+C`

### Optional / extension features
- HC-SR04 ultrasonic proximity sensing
- Buzzer alarm
- Local access logging

### Removed from final scope
- NFC authentication

The original concept considered NFC as one of the authentication methods, but the final scope was refined to focus on a more reliable and achievable prototype using magstripe input, face verification, and relay control.

---

## 3. Realtime Requirements

This project is designed as a realtime embedded prototype.  
The main realtime requirements include:

- responding quickly after a card is presented
- updating LEDs immediately after authentication
- activating the relay in time when access is granted
- returning the system to the idle state after temporary status display
- avoiding unnecessary blocking in the main control flow
- handling shutdown safely and predictably

The software is structured around event-driven processing, callbacks, timers, and modular components rather than a single monolithic flow.

---

## 4. System Workflow

The system follows an event-driven workflow:

### Workflow Steps
- A card is swiped through the magstripe reader
- The input is captured and processed in realtime
- The card data is verified against the allowlist
- If valid, a signal is sent to unlock the door via relay
- LED indicators update immediately to reflect the access status
- The system returns to idle state after processing

This workflow ensures low latency and avoids blocking operations by using callbacks and modular components.

---

## 5. Hardware Used

### Main hardware
- Raspberry Pi 5
- USB magstripe reader
- Camera module 
- Relay module
- Red LED
- Yellow LED
- Green LED
- Breadboard
- Jumper wires
- Power supply

### Optional hardware
- HC-SR04 ultrasonic sensor
- Buzzer
- Reed switch / door sensor

---

## 6. Software Design

The system is implemented using modular C++ components.

### Main modules
- `MagstripeReader`  
  Reads raw data from the USB magstripe reader.

- `CardVerifier`  
  Verifies whether the presented card is in the allowlist.

- `EventBus`  
  Dispatches authentication result events to the corresponding modules.

- `StatusLeds`  
  Controls LED states for `idle`, `granted`, and `denied`.

- `main.cpp`  
  Integrates the system flow, event handling, reader thread, and shutdown logic.

### Design characteristics
- object-oriented
- modular
- event-driven
- callback-based
- Raspberry Pi GPIO integration
- designed for realtime response

---

## 7. Build Instructions

### Requirements
- Raspberry Pi running Linux
- C++ compiler with CMake support
- `libgpiod` installed

### Build
```bash
rm -rf build
cmake -S . -B build
cmake --build build -j


