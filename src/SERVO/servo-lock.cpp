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
    target_open_ = false;
    relock_pending_ = false;
  }
  cv_.notify_one();
}

void ServoLock::unlock() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    target_open_ = true;
    schedule_relock_locked();
  }
  cv_.notify_one();
}

void ServoLock::set_hold_ms(int hold_ms) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    hold_ms_ = std::max(hold_ms, 0);
    if (target_open_) {
      schedule_relock_locked();
    }
  }
  cv_.notify_one();
}

void ServoLock::schedule_relock_locked() {
  relock_pending_ = true;
  relock_deadline_ =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(hold_ms_);
}

void ServoLock::emit_pwm_burst(int pulse_us, int burst_ms) {
  pulse_us =
      std::clamp(pulse_us, kMinPulseUs, std::min(kMaxPulseUs, period_us_ - 500));
  const int low_us = std::max(period_us_ - pulse_us, 1000);

  const auto stop_time =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(burst_ms);

  while (!stop_.load() && std::chrono::steady_clock::now() < stop_time) {
    signal_.on();
    std::this_thread::sleep_for(std::chrono::microseconds(pulse_us));

    signal_.off();
    std::this_thread::sleep_for(std::chrono::microseconds(low_us));
  }

  // Stop pulses after movement to reduce jitter.
  signal_.off();
}

void ServoLock::worker_loop() {
  std::unique_lock<std::mutex> lock(mtx_);

  bool applied_open = false;
  bool first_apply = true;

  while (!stop_.load()) {
    if (first_apply || target_open_ != applied_open) {
      const bool desired_open = target_open_;
      applied_open = desired_open;
      first_apply = false;

      lock.unlock();
      emit_pwm_burst(desired_open ? open_pulse_us_ : closed_pulse_us_, 450);
      lock.lock();
      continue;
    }

    if (relock_pending_) {
      const auto deadline = relock_deadline_;

      cv_.wait_until(lock, deadline, [&]() {
        return stop_.load() ||
               !relock_pending_ ||
               relock_deadline_ != deadline ||
               target_open_ != applied_open;
      });

      if (stop_.load()) {
        break;
      }

      if (relock_pending_ &&
          std::chrono::steady_clock::now() >= relock_deadline_) {
        target_open_ = false;
        relock_pending_ = false;
      }
    } else {
      cv_.wait(lock, [&]() {
        return stop_.load() ||
               relock_pending_ ||
               target_open_ != applied_open;
      });
    }
  }

  signal_.off();
}