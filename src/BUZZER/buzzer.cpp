#include "BUZZER/buzzer.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Buzzer::Buzzer(const char* chip_name, int buzzer_gpio, const char* consumer)
    : buzzer_(chip_name, buzzer_gpio, consumer) {
  // 低电平触发：
  // 高电平 = 不响
  // 低电平 = 响
  buzzer_.on();
  worker_ = std::thread([this]() { worker_loop(); });
}

Buzzer::~Buzzer() {
  stop_.store(true);
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  // 退出时保持静音
  buzzer_.on();
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
        // 响一下：低电平触发
        buzzer_.off();
        std::this_thread::sleep_for(120ms);
        buzzer_.on();
        break;

      case Mode::Denied:
        // 三声
        for (int i = 0; i < 3; ++i) {
          buzzer_.off();
          std::this_thread::sleep_for(120ms);
          buzzer_.on();
          std::this_thread::sleep_for(100ms);
        }
        break;

      case Mode::PendingFace:
        // 两声提示
        for (int i = 0; i < 2; ++i) {
          buzzer_.off();
          std::this_thread::sleep_for(80ms);
          buzzer_.on();
          std::this_thread::sleep_for(120ms);
        }
        break;

      case Mode::Off:
      default:
        // 静音：高电平
        buzzer_.on();
        break;
    }
  }
}