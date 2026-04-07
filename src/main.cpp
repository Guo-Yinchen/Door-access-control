#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"
#include "verifier/verifier.hpp"
#include "RIsk/risk-policy.hpp"
#include "Camera/camera-stream.hpp"
#include "FACE/face-verifier.hpp"
#include "Telemetry/auth-trace.hpp"

#if ENABLE_GPIO
#include "LED/led-v1.hpp"
#include "Magnetic-reader/Magnetic-reader.hpp"
#include "BUZZER/buzzer.hpp"
#include "SERVO/servo-lock.hpp"
#endif

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
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

struct FaceRequest {
  std::string card_id;
  std::shared_ptr<AuthTrace> trace;
};

class FaceTaskSlot {
public:
  bool submit_if_idle(FaceRequest req) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (stop_ || pending_ || verifying_) {
        return false;
      }

      pending_req_ = std::move(req);
      pending_ = true;
    }

    cv_.notify_one();
    return true;
  }

  bool wait_and_take(FaceRequest& req, const std::atomic<bool>& stop_requested) {
    std::unique_lock<std::mutex> lock(mtx_);

    cv_.wait(lock, [&] {
      return stop_ || stop_requested.load() || pending_;
    });

    if ((stop_ || stop_requested.load()) && !pending_) {
      return false;
    }

    req = std::move(pending_req_);
    pending_req_ = FaceRequest{};
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
  FaceRequest pending_req_;
  bool pending_{false};
  bool verifying_{false};
  bool stop_{false};
};
} // namespace

int main() {
  try {
    std::signal(SIGINT, handle_sigint);
    std::atomic<uint64_t> next_trace_id{1};

#if ENABLE_GPIO
    const char* chip = "gpiochip0";

    const int RED_GPIO = 17;
    const int YELLOW_GPIO = 27;
    const int GREEN_GPIO = 22;
    const int BUZZER_GPIO = 18;
    const int SERVO_GPIO = 12;
    const int SERVO2_GPIO = 13;
#endif

    EventBus bus;

#if ENABLE_GPIO
    StatusLeds leds(chip, RED_GPIO, YELLOW_GPIO, GREEN_GPIO, "door_control");
    Buzzer buzzer(chip, BUZZER_GPIO, "door_buzzer");
    ServoLock lock(chip, SERVO_GPIO, "door_servo", 500, 1500, 20000, 3000);
    ServoLock lock2(chip, SERVO2_GPIO, "door_servo2", 500, 1500, 20000, 3000);
    MagstripeReader reader;
#endif

    CardVerifier verifier("mag-cards_allowlist.txt");
    RiskPolicy risk_policy;

    CameraStream camera_stream(CameraStream::Config{
        640,
        480,
        10,
        200,
        0
    });

    if (!camera_stream.start()) {
      std::cerr << "[CAM] Failed to start CSI camera stream.\n";
      return 1;
    }

    FaceVerifier face_verifier(camera_stream);
    FaceTaskSlot face_slot;

#if ENABLE_GPIO
    leds.attach(bus, 2000);
    buzzer.attach(bus);
    lock.attach(bus);
    lock2.attach(bus);
#endif

    std::thread bus_thread([&]() {
      bus.dispatch_loop();
    });

    std::thread face_thread([&]() {
      FaceRequest req;

      while (face_slot.wait_and_take(req, g_stop_requested)) {
        const bool face_ok = face_verifier.verify(req.card_id, g_stop_requested);

        const auto now = AuthTrace::clock::now();
        if (req.trace) {
          req.trace->t_face_result = now;
          req.trace->t_final_feedback = now;
          req.trace->log_face_final(face_ok);
        }

        face_slot.finish_current();

        if (g_stop_requested.load()) {
          break;
        }

#if ENABLE_GPIO
        bus.publish(face_ok ? AuthResult::granted : AuthResult::denied,
                    Target::LED | Target::LOCK | Target::BUZZER);
#else
        (void)face_ok;
#endif
      }
    });

#if ENABLE_GPIO
    bus.publish(AuthResult::idle, Target::LED | Target::LOCK | Target::BUZZER);
    std::cout << "Swipe card now (Ctrl+C to exit)\n";
#else
    std::cout << "GPIO disabled build running (Ctrl+C to exit)\n";
#endif

#if ENABLE_GPIO
    std::thread reader_thread([&]() {
      reader.run([&](const std::string& raw) {
        if (g_stop_requested.load()) {
          return;
        }

        auto trace = std::make_shared<AuthTrace>();
        trace->trace_id = next_trace_id.fetch_add(1);
        trace->t_card_read = AuthTrace::clock::now();

        std::string card_id;
        const bool card_ok = verifier.verify(raw, card_id);

        trace->t_card_verified = AuthTrace::clock::now();
        trace->card_id = card_id;
        trace->card_valid = card_ok;

        std::cout << "[RAW]  " << raw << "\n";
        std::cout << (card_ok ? "[OK]   " : "[FAIL] ") << card_id << "\n";

        if (!card_ok) {
          trace->t_final_feedback = AuthTrace::clock::now();
          trace->log_invalid_denied();

          bus.publish(AuthResult::denied,
                      Target::LED | Target::LOCK | Target::BUZZER);
          return;
        }

        if (risk_policy.require_face_now()) {
          trace->high_risk = true;

          const bool submitted = face_slot.submit_if_idle(FaceRequest{card_id, trace});
          if (!submitted) {
            std::cout << "[FACE] Face verification busy. New card ignored.\n";
            return;
          }

          trace->t_pending_face = AuthTrace::clock::now();
          trace->log_pending_face();

          std::cout << "[RISK] High-risk condition detected. Face verification required.\n";
          bus.publish(AuthResult::pending_face,
                      Target::LED | Target::LOCK | Target::BUZZER);
          return;
        }

        trace->t_final_feedback = AuthTrace::clock::now();
        trace->log_valid_granted();

        bus.publish(AuthResult::granted,
                    Target::LED | Target::LOCK | Target::BUZZER);
      });
    });
#endif

    while (!g_stop_requested.load()) {
      ::pause();
    }

#if ENABLE_GPIO
    reader.stop();
#endif
    face_slot.shutdown();
    camera_stream.stop();

#if ENABLE_GPIO
    if (reader_thread.joinable()) {
      reader_thread.join();
    }
#endif

    if (face_thread.joinable()) {
      face_thread.join();
    }

#if ENABLE_GPIO
    bus.publish(AuthResult::idle, Target::LED | Target::LOCK | Target::BUZZER);
#endif
    bus.stop();

    if (bus_thread.joinable()) {
      bus_thread.join();
    }

#if ENABLE_GPIO
    leds.all_off();
    buzzer.all_off();
#endif

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}
