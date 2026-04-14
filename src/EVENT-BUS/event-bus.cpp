#include "EVENT/event-bus.hpp"
// EventBus 实现一个简单的事件总线，支持订阅和发布认证事件，并在独立线程中分发事件给订阅者
// EventBus implements a simple event bus that supports subscribing and publishing authentication events, and dispatches
void EventBus::subscribe(Target my_target, Handler handler) {
  std::lock_guard<std::mutex> lock(mtx_);
  subs_.push_back(Sub{static_cast<uint32_t>(my_target), std::move(handler)});
}
// publish 发布一个事件，指定认证结果和目标设备，事件会被分发给所有订阅了相关目标的处理函数
// publish publishes an event with a specified authentication result and target devices, which will be dispatched to all handlers subscribed to the relevant targets
void EventBus::publish(AuthResult r, uint32_t targets) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    q_.push(AuthEvent{r, targets});
  }
  cv_.notify_one();
}
// dispatch_loop 是事件总线的主循环，等待事件并分发给订阅者处理
// dispatch_loop is the main loop of the event bus, waiting for events and dispatching them to subscribers for processing
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
// stop 设置停止标志并通知所有等待的线程，确保系统能够干净地关闭
// stop sets the stop flag and notifies all waiting threads to ensure a clean shutdown of the
void EventBus::stop() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_ = true;
  }
  cv_.notify_all();
}