#include "LED/led-v1.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "verifier/verifier.hpp"

#include <iostream>

int main() {
  try {
    const char* chip = "gpiochip0";

    // BCM GPIO 编号
    const int RED_GPIO = 17;
    const int YELLOW_GPIO = 27;
    const int GREEN_GPIO = 22;

    EventBus bus;
    StatusLeds leds(chip, RED_GPIO, YELLOW_GPIO, GREEN_GPIO, "door_control");

    // 接入 EventBus：GRANTED/DENIED 显示 2 秒后自动回 idle
    leds.attach(bus, 2000);

    // 启动先进入 idle（黄灯常亮）
    bus.publish(AuthResult::idle, Target::LED);
    bus.poll();

    // 读卡器：键盘模拟读卡器对应 /dev/input/event9
    MagstripeReader reader;

    // 验证器：白名单文件 cards_allowlist.txt，每行一个卡号，# 开头的行会被忽略
    CardVerifier verifier("cards_allowlist.txt");

    std::cout << "Swipe card now (Ctrl+C to exit)\n";

    // 阻塞循环
    reader.run([&](const std::string& raw) {
      std::string card_id;
      const bool ok = verifier.verify(raw, card_id);

      std::cout << "[RAW]  " << raw << "\n";
      std::cout << (ok ? "[OK]   " : "[FAIL] ") << card_id << "\n";

      bus.publish(ok ? AuthResult::granted : AuthResult::denied, Target::LED);
      bus.poll();  // 立即分发给 LED
    });

    leds.all_off();
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}