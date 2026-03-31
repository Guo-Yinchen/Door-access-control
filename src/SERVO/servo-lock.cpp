#include "SERVO/servo-lock.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
constexpr int kMinPulseUs = 500;
constexpr int kMaxPulseUs = 2400;
}

ServoLock::ServoLock(const char* chip_name,
                     int signal_gpio,
                     const char* consumer,
                     int closed_pulse_us,
                     int open_pulse_us,
                     int period_us,
                     int unlock_hold_ms)
    : signal_(chip_name, signal_gpio, consumer),
      closed_pulse_us_(std::clamp(closed_pulse_us, kMinPulseUs, kMaxPulseUs)),
      open_pulse_us_(std::clamp(open_pulse_us, kMinPulseUs, kMaxPulseUs)),
      period_us_(std::max(period_us, 5000)),
      current_pulse_us_(closed_pulse_us_),
      hold_ms_(std::max(unlock_hold_ms, 0)) {
  worker_ = std::thread([this]() { worker_loop(); });
}

ServoLock::~ServoLock() {
  stop_.store(true);
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  signal_.off();
}

void ServoLock::attach(EventBus& bus) {
  bus.subscribe(Target::LOCK, [this](const AuthEvent& e) {
    switch (e.result) {
      case AuthResult::granted:
        unlock();
        break;

      case AuthResult::denied:
      case AuthResult::idle:
      case AuthResult::pending_face:
      default:
        lock();
        break;
    }
  });
}

void ServoLock::lock() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    current_pulse_us_ = closed_pulse_us_;
    relock_pending_ = false;
  }
  cv_.notify_one();
}

void ServoLock::unlock() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    current_pulse_us_ = open_pulse_us_;
    schedule_relock();
  }
  cv_.notify_one();
}

void ServoLock::set_hold_ms(int hold_ms) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    hold_ms_ = std::max(hold_ms, 0);
    if (current_pulse_us_ == open_pulse_us_) {
      schedule_relock();
    }
  }
  cv_.notify_one();
}

void ServoLock::schedule_relock() {
  relock_pending_ =
      true;
  relock_deadline_ =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(hold_ms_);
}

void ServoLock::worker_loop() {
  while (!stop_.load()) {
    int pulse_us = closed_pulse_us_;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      pulse_us = current_pulse_us_;

      if (relock_pending_ &&
          std::chrono::steady_clock::now() >= relock_deadline_) {
        current_pulse_us_ = closed_pulse_us_;
        relock_pending_ = false;
        pulse_us = current_pulse_us_;
      }
    }

    pulse_us =
        std::clamp(pulse_us, kMinPulseUs, std::min(kMaxPulseUs, period_us_ - 500));
    const int low_us = std::max(period_us_ - pulse_us, 1000);

    signal_.on();
    std::this_thread::sleep_for(std::chrono::microseconds(pulse_us));

    signal_.off();
    std::this_thread::sleep_for(std::chrono::microseconds(low_us));
  }
}