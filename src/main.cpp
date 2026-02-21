#include "LED/led-v1.hpp"
#include "GPIO/gpio-line.hpp"
#include <chrono>
#include <thread>
#include <iostream>

int main() {
  try {
    const char* chip = "gpiochip0";

    // （BCM GPIO 编号）
    const int RED_GPIO = 17;
    const int YELLOW_GPIO = 27;
    const int GREEN_GPIO = 22;

    StatusLeds leds(chip, RED_GPIO, YELLOW_GPIO, GREEN_GPIO, "door_control");

    leds.idle();
    std::cout << "Idle\n";

    // 模拟事件
    std::this_thread::sleep_for(std::chrono::seconds(2));
    leds.granted();
    std::cout << "Granted\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    leds.idle();
    std::cout << "Back to idle\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    leds.denied();
    std::cout << "Denied\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    leds.idle();
    std::cout << "Exit\n";
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}