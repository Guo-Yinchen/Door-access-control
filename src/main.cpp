#include "LED/led-v1.hpp"
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "verifier/verifier.hpp"
#include "RIsk/risk-policy.hpp"
#include "FACE/face-verifier.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {
std::atomic<bool> g_stop_requested{false};

void handle_sigint(int) {
  g_stop_requested.store(true);
}

class FaceTaskSlot {
public:
  bool submit_if_idle(std::string card_id) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (stop_ || pending_ || verifying_) {
        return false;
      }

      pending_card_id_ = std::move(card_id);
      pending_ = true;
    }

    cv_.notify_one();
    return true;
  }

  bool wait_and_take(std::string& card_id, const std::atomic<bool>& stop_requested) {
    std::unique_lock<std::mutex> lock(mtx_);

    cv_.wait(lock, [&] {
      return stop_ || stop_requested.load() || pending_;
    });

    if ((stop_ || stop_requested.load()) && !pending_) {
      return false;
    }

    card_id = std::move(pending_card_id_);
    pending_card_id_.clear();
    pending_ = false;
    verifying_ = true;
    return true;
  }

  void finish_current() {
    std::lock_guard<std::mutex> lock(mtx_);
    verifying_ = false;
  }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      stop_ = true;
    }
    cv_.notify_all();
  }

private:
  std::mutex mtx_;
  std::condition_variable cv_;
  std::string pending_card_id_;
  bool pending_{false};
  bool verifying_{false};
  bool stop_{false};
};
} // namespace

int main() {
  try {
    std::signal(SIGINT, handle_sigint);

    const char* chip = "gpiochip0";

    const int RED_GPIO = 17;
    const int YELLOW_GPIO = 27;
    const int GREEN_GPIO = 22;

    EventBus bus;
    StatusLeds leds(chip, RED_GPIO, YELLOW_GPIO, GREEN_GPIO, "door_control");
    MagstripeReader reader;
    CardVerifier verifier("mag-cards_allowlist.txt");
    RiskPolicy risk_policy;
    FaceVerifier face_verifier;
    FaceTaskSlot face_slot;

    leds.attach(bus, 2000);

    std::thread bus_thread([&]() {
      bus.dispatch_loop();
    });

    std::thread face_thread([&]() {
      std::string card_id;

      while (face_slot.wait_and_take(card_id, g_stop_requested)) {
        const bool face_ok = face_verifier.verify(card_id, g_stop_requested);

        face_slot.finish_current();

        if (g_stop_requested.load()) {
          break;
        }

        bus.publish(face_ok ? AuthResult::granted : AuthResult::denied,
                    Target::LED | Target::LOCK);
      }
    });

    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);

    std::cout << "Swipe card now (Ctrl+C to exit)\n";

    std::thread reader_thread([&]() {
      reader.run([&](const std::string& raw) {
        if (g_stop_requested.load()) {
          return;
        }

        std::string card_id;
        const bool card_ok = verifier.verify(raw, card_id);

        std::cout << "[RAW]  " << raw << "\n";
        std::cout << (card_ok ? "[OK]   " : "[FAIL] ") << card_id << "\n";

        if (!card_ok) {
          bus.publish(AuthResult::denied, Target::LED | Target::LOCK);
          return;
        }

        if (risk_policy.require_face_now()) {
          const bool submitted = face_slot.submit_if_idle(card_id);

          if (!submitted) {
            std::cout << "[FACE] Face verification busy. New card ignored.\n";
            return;
          }

          std::cout << "[RISK] High-risk condition detected. Face verification required.\n";
          bus.publish(AuthResult::pending_face, Target::LED | Target::LOCK);
          return;
        }

        bus.publish(AuthResult::granted, Target::LED | Target::LOCK);
      });
    });

    while (!g_stop_requested.load()) {
      ::pause();
    }

    reader.stop();
    face_slot.shutdown();

    if (reader_thread.joinable()) {
      reader_thread.join();
    }
    if (face_thread.joinable()) {
      face_thread.join();
    }

    bus.publish(AuthResult::idle, Target::LED | Target::LOCK);
    bus.stop();

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