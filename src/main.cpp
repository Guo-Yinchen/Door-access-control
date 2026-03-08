#include "LED/led-v1.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "verifier/verifier.hpp"
#include "RIsk/risk-policy.hpp"
#include "FACE/face-verifier.hpp"

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
    MagstripeReader reader;
    CardVerifier verifier("mag-cards_allowlist.txt");
    RiskPolicy risk_policy;
    FaceVerifier face_verifier;

    leds.attach(bus, 2000);

    // 启动先进入 idle
    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);
    bus.poll();

    std::cout << "Swipe card now (Ctrl+C to exit)\n";

    std::thread reader_thread([&]() {
      reader.run([&](const std::string& raw) {
        std::string card_id;
        const bool card_ok = verifier.verify(raw, card_id);

        std::cout << "[RAW]  " << raw << "\n";
        std::cout << (card_ok ? "[OK]   " : "[FAIL] ") << card_id << "\n";

        if (!card_ok) {
          bus.publish(AuthResult::denied, Target::LED | Target::LOCK);
          return;
        }

        // 高危模式：要求人脸验证
        if (risk_policy.require_face_now()) {
          std::cout << "[RISK] High-risk condition detected. Face verification required.\n";
          bus.publish(AuthResult::pending_face, Target::LED | Target::LOCK);
          bus.poll();

          const bool face_ok = face_verifier.verify(card_id);
          bus.publish(face_ok ? AuthResult::granted : AuthResult::denied,
                      Target::LED | Target::LOCK);
          return;
        }

        // 普通模式：刷卡通过即可
        bus.publish(AuthResult::granted, Target::LED | Target::LOCK);
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

    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);
    bus.poll();
    leds.all_off();

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}