# Testing

## 1. Test Overview

This project has been tested to ensure correct realtime behavior and system responsiveness.

## 2. Functional Tests

- Swipe a valid card → system grants access and unlocks the door
- Swipe an invalid card → system denies access and keeps door locked
- LED indicators correctly display status (idle, granted, denied)

## 3. Realtime Behavior

- System responds immediately after card input
- No blocking delays are observed during operation
- Events are handled using callbacks and modular components

## 4. Stability Tests

- Multiple card inputs processed continuously without crash
- System returns to idle state correctly after each operation

## 5. Summary

The system demonstrates stable and reliable realtime performance under normal operating conditions.
