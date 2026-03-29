#include "EVENT/event-bus.hpp"

void EventBus::subscribe(Target my_target, Handler handler) {
  std::lock_guard<std::mutex> lock(mtx_);
  subs_.push_back(Sub{static_cast<uint32_t>(my_target), std::move(handler)});
}

void EventBus::publish(AuthResult r, uint32_t targets) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    q_.push(AuthEvent{r, targets});
  }
  cv_.notify_one();
}

void EventBus::dispatch_loop() {
  std::unique_lock<std::mutex> lock(mtx_);

  while (true) {
    cv_.wait(lock, [this] { return stop_ || !q_.empty(); });

    if (stop_ && q_.empty()) {
      return;
    }

    AuthEvent ev = q_.front();
    q_.pop();
    auto snapshot = subs_;

    lock.unlock();
    for (auto& s : snapshot) {
      if ((s.target & ev.targets) != 0) {
        s.handler(ev);
      }
    }
    lock.lock();
  }
}

void EventBus::stop() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_ = true;
  }
  cv_.notify_all();
}