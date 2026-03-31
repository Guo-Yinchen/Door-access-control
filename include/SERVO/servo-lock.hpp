#pragma once

#include "EVENT/event-bus.hpp"
#include "GPIO/gpio-line.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class ServoLock {
public:
  ServoLock(const char* chip_name,
            int signal_gpio,
            const char* consumer = "servo_lock",
            int closed_pulse_us = 500,
            int open_pulse_us = 1500,
            int period_us = 20000,
            int unlock_hold_ms = 3000);
  ~ServoLock();

  ServoLock(const ServoLock&) = delete;
  ServoLock& operator=(const ServoLock&) = delete;

  void attach(EventBus& bus);

  void lock();
  void unlock();
  void set_hold_ms(int hold_ms);

private:
  void worker_loop();
  void schedule_relock();

private:
  GpioLine signal_;

  const int closed_pulse_us_;
  const int open_pulse_us_;
  const int period_us_;

  std::atomic<bool> stop_{false};
  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread worker_;

  int current_pulse_us_;
  int hold_ms_;
  bool relock_pending_{false};
  std::chrono::steady_clock::time_point relock_deadline_{};
};