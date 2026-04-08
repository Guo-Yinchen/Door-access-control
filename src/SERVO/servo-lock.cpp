#include "SERVO/servo-lock.hpp"

#include <pigpio.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
constexpr int kMinPulseUs = 500;
constexpr int kMaxPulseUs = 2400;
constexpr auto kSettleTime = std::chrono::milliseconds(400);
}

std::mutex ServoLock::pigpio_mtx_;
int ServoLock::pigpio_users_ = 0;

ServoLock::ServoLock(const char* chip_name,
                     int signal_gpio,
                     const char* consumer,
                     int closed_pulse_us,
                     int open_pulse_us,
                     int period_us,
                     int unlock_hold_ms)
    : signal_gpio_(signal_gpio),
      closed_pulse_us_(std::clamp(closed_pulse_us, kMinPulseUs, kMaxPulseUs)),
      open_pulse_us_(std::clamp(open_pulse_us, kMinPulseUs, kMaxPulseUs)),
      hold_ms_(std::max(unlock_hold_ms, 0)) {
  (void)chip_name;
  (void)consumer;
  (void)period_us;

  std::lock_guard<std::mutex> guard(pigpio_mtx_);
  if (pigpio_users_ == 0) {
    if (gpioInitialise() < 0) {
      throw std::runtime_error("pigpio initialisation failed");
    }
  }
  ++pigpio_users_;

  gpioSetMode(signal_gpio_, PI_OUTPUT);
  gpioServo(signal_gpio_, 0);

  worker_ = std::thread([this]() { worker_loop(); });
}

ServoLock::~ServoLock() {
  stop_.store(true);
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  gpioServo(signal_gpio_, 0);

  std::lock_guard<std::mutex> guard(pigpio_mtx_);
  --pigpio_users_;
  if (pigpio_users_ == 0) {
    gpioTerminate();
  }
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

void ServoLock::move_and_release(bool open) {
  const int pulse_us = open ? open_pulse_us_ : closed_pulse_us_;

  gpioServo(signal_gpio_, pulse_us);
  std::this_thread::sleep_for(kSettleTime);

  // Stop sending servo pulses after movement to reduce jitter.
  gpioServo(signal_gpio_, 0);
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
      move_and_release(desired_open);
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

  gpioServo(signal_gpio_, 0);
}