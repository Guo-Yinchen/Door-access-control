#include "LED/led-v1.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "verifier/verifier.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_stop_requested{false};

void handle_sigint(int) {
  g_stop_requested.store(true);
}
}

int main() {
  try {
    std::signal(SIGINT, handle_sigint);

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

    // 验证器：白名单文件 mag-cards_allowlist.txt
    CardVerifier verifier("mag-cards_allowlist.txt");

    std::cout << "Swipe card now (Ctrl+C to exit)\n";

    std::thread reader_thread([&]() {
      reader.run([&](const std::string& raw) {
        std::string card_id;
        const bool ok = verifier.verify(raw, card_id);

        std::cout << "[RAW]  " << raw << "\n";
        std::cout << (ok ? "[OK]   " : "[FAIL] ") << card_id << "\n";

        bus.publish(ok ? AuthResult::granted : AuthResult::denied, Target::LED);
      });
    });

    while (!g_stop_requested.load()) {
      bus.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    reader.stop();
    if (reader_thread.joinable()) {
      reader_thread.join();
    }

    bus.publish(AuthResult::idle, Target::LED);
    bus.poll();
    leds.all_off();

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}