#include "BUZZER/buzzer.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Buzzer::Buzzer(const char* chip_name, int buzzer_gpio, const char* consumer)
    : buzzer_(chip_name, buzzer_gpio, consumer, true) {
  // active-low:
  // logical ACTIVE   -> physical LOW  -> buzzer sounds
  // logical INACTIVE -> physical HIGH -> buzzer silent
  buzzer_.off();  // keep silent at startup
  worker_ = std::thread([this]() { worker_loop(); });
}

Buzzer::~Buzzer() {
  stop_.store(true);
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  // keep silent before underlying line is released
  //保持静音状态
  buzzer_.off();
}

void Buzzer::attach(EventBus& bus) {
  bus.subscribe(Target::BUZZER, [this](const AuthEvent& e) {
    switch (e.result) {
      case AuthResult::granted:
        granted();
        break;
      case AuthResult::denied:
        denied();
        break;
      case AuthResult::pending_face:
        pending_face();
        break;
      case AuthResult::idle:
      default:
        all_off();
        break;
    }
  });
}

void Buzzer::all_off() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = Mode::Off;
  }
  cv_.notify_one();
}

void Buzzer::granted() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = Mode::Granted;
  }
  cv_.notify_one();
}

void Buzzer::denied() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = Mode::Denied;
  }
  cv_.notify_one();
}

void Buzzer::pending_face() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = Mode::PendingFace;
  }
  cv_.notify_one();
}

void Buzzer::worker_loop() {
  while (!stop_.load()) {
    Mode current = Mode::Off;

    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this]() {
        return stop_.load() || mode_ != Mode::Off;
      });

      if (stop_.load()) {
        break;
      }

      current = mode_;
      mode_ = Mode::Off;
    }

    switch (current) {
      case Mode::Granted:
        // one short beep
        //  一声短促蜂鸣，持续120ms
        buzzer_.on();
        std::this_thread::sleep_for(120ms);
        buzzer_.off();
        break;

      case Mode::Denied:
        // three beeps
        for (int i = 0; i < 3; ++i) {
          buzzer_.on();
          std::this_thread::sleep_for(120ms);
          buzzer_.off();
          std::this_thread::sleep_for(100ms);
        }
        break;

      case Mode::PendingFace:
        // two short beeps
        for (int i = 0; i < 2; ++i) {
          buzzer_.on();
          std::this_thread::sleep_for(80ms);
          buzzer_.off();
          std::this_thread::sleep_for(120ms);
        }
        break;

      case Mode::Off:
      default:
        buzzer_.off();
        break;
    }
  }

  // extra safety on worker exit
  buzzer_.off();
}