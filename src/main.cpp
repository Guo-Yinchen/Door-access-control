#include "LED/led-v1.hpp"
#include "GPIO/gpio-line.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
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

    EventBus bus;
    StatusLeds leds(chip, RED_GPIO, YELLOW_GPIO, GREEN_GPIO, "door_control");

    // 接入 EventBus：GRANTED/DENIED 显示 2 秒后自动回 idle
    leds.attach(bus, 2000);

    std::cout << "Idle\n";

    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    bool sent_granted = false;
    bool sent_denied  = false;
    bool sent_idle    = false;

    while (true) {
      auto now = clock::now();
      auto t = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

      // 模拟：2秒后发 GRANTED（只发给 LED）
      if (!sent_granted && t >= 2) {
        bus.publish(AuthResult::granted, Target::LED);
        std::cout << "Publish: GRANTED\n";
        sent_granted = true;
      }

      // 模拟：6秒后发 DENIED（只发给 LED）
      if (!sent_denied && t >= 6) {
        bus.publish(AuthResult::denied, Target::LED);
        std::cout << "Publish: DENIED\n";
        sent_denied = true;
      }
    
      if (!sent_granted && t >= 8) {
        bus.publish(AuthResult::idle, Target::LED);
        std::cout << "Publish: IDLE\n";
        sent_idle = true;
      }

      // 分发事件 + LED
      bus.poll();

      // 演示结束条件：发完 denied 后再过 3 秒退出
      if (sent_denied && t >= 10) {
        std::cout << "Exit\n";
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    leds.all_off();
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}