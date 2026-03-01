#include "EVENT/event-bus.hpp"

void EventBus::subscribe(Target my_target, Handler handler) {
  std::lock_guard<std::mutex> lock(mtx_);
  subs_.push_back(Sub{static_cast<uint32_t>(my_target), std::move(handler)});
}

void EventBus::publish(AuthResult r, uint32_t targets) {
  std::lock_guard<std::mutex> lock(mtx_);
  q_.push(AuthEvent{r, targets});
}

void EventBus::poll() {
  AuthEvent ev;
  std::vector<Sub> snapshot;

  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (q_.empty()) return;
    ev = q_.front();
    q_.pop();

    // 拷贝一份订阅者列表，避免回调里再订阅导致迭代器问题
    snapshot = subs_;
  }

  for (auto& s : snapshot) {
    if ((s.target & ev.targets) != 0) { //  定向过滤
      s.handler(ev);
    }
  }
}