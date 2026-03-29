#include "LED/led-v1.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "verifier/verifier.hpp"
#include "RIsk/risk-policy.hpp"
#include "FACE/face-verifier.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <unistd.h>

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

    std::thread bus_thread([&]() {
      bus.dispatch_loop();
    });

    // 启动先进入 idle
    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);

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

        if (risk_policy.require_face_now()) {
          std::cout << "[RISK] High-risk condition detected. Face verification required.\n";
          bus.publish(AuthResult::pending_face, Target::LED | Target::LOCK);

          const bool face_ok = face_verifier.verify(card_id);
          bus.publish(face_ok ? AuthResult::granted : AuthResult::denied,
                      Target::LED | Target::LOCK);
          return;
        }

        bus.publish(AuthResult::granted, Target::LED | Target::LOCK);
      });
    });

    while (!g_stop_requested.load()) {
      ::pause();
    }

    reader.stop();
    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);
    bus.stop();

    if (reader_thread.joinable()) {
      reader_thread.join();
    }
    if (bus_thread.joinable()) {
      bus_thread.join();
    }

    leds.all_off();
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}