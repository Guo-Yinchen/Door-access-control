#include "LED/led-v1.hpp"

StatusLeds::StatusLeds(const char* chip_name,
                       int red_gpio, int yellow_gpio, int green_gpio,
                       const char* consumer)
  : red_(chip_name, red_gpio, consumer),
    yellow_(chip_name, yellow_gpio, consumer),
    green_(chip_name, green_gpio, consumer) {
  start_timer_worker();
}

StatusLeds::~StatusLeds() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_worker_ = true;
    pending_idle_ = false;
  }
  cv_.notify_all();

  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }
}
void StatusLeds::pending_face() {
  red_.on();
  yellow_.on();
  green_.on();
}
void StatusLeds::idle() {
  red_.off();
  green_.off();
  yellow_.on();
}

void StatusLeds::granted() {
  red_.off();
  yellow_.on();
  green_.on();
}

void StatusLeds::denied() {
  green_.off();
  yellow_.on();
  red_.on();
}

void StatusLeds::all_off() {
  red_.off();
  yellow_.off();
  green_.off();
}

void StatusLeds::start_timer_worker() {
  timer_thread_ = std::thread([this]() {
    std::unique_lock<std::mutex> lock(mtx_);

    while (!stop_worker_) {
      cv_.wait(lock, [this] { return stop_worker_ || pending_idle_; });
      if (stop_worker_) break;

      const auto deadline = deadline_;

      const bool interrupted = cv_.wait_until(lock, deadline, [this, deadline] {
        return stop_worker_ || !pending_idle_ || deadline_ != deadline;
      });

      if (stop_worker_) break;
      if (interrupted) {
        continue; // 被取消或重新安排了
      }

      pending_idle_ = false;
      lock.unlock();
      idle();
      lock.lock();
    }
  });
}

void StatusLeds::schedule_idle() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_idle_ = true;
    deadline_ = std::chrono::steady_clock::now() + hold_;
  }
  cv_.notify_all();
}

void StatusLeds::cancel_pending_idle() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_idle_ = false;
  }
  cv_.notify_all();
}

void StatusLeds::attach(EventBus& bus, int hold_ms) {
  hold_ = std::chrono::milliseconds(hold_ms);

  bus.subscribe(Target::LED, [this](const AuthEvent& e) {
    switch (e.result) {
      case AuthResult::granted:
        granted();
        schedule_idle();
        break;

      case AuthResult::denied:
        denied();
        schedule_idle();
        break;

      case AuthResult::idle:
        cancel_pending_idle();
        idle();
        break;

      case AuthResult::pending_face:
        pending_face();
        break;

    }
  });
}

void StatusLeds::tick() {
}