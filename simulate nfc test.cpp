#include <iostream>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

// 事件：只做一个 NFC UID
struct NfcEvent {
  std::string uid;
  std::chrono::steady_clock::time_point t; // 事件产生时间（用于算延迟）
};

// 线程安全队列：事件总线
class EventBus {
public:
  void push(NfcEvent e) {
    {
      std::lock_guard<std::mutex> lk(m_);
      q_.push(std::move(e));
    }
    cv_.notify_one();
  }

  // 阻塞等待一个事件
  bool wait_pop(NfcEvent& out) {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]{ return stop_ || !q_.empty(); });

    if (stop_ && q_.empty()) return false;

    out = std::move(q_.front());
    q_.pop();
    return true;
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lk(m_);
      stop_ = true;
    }
    cv_.notify_all();
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  std::queue<NfcEvent> q_;
  bool stop_ = false;
};

// 白名单：先写死，后面再换文件/数据库
bool is_allowed(const std::string& uid) {
  return uid == "1234" || uid == "A1B2C3" || uid == "TEST";
}

int main() {
  EventBus bus;

  // 控制线程：事件驱动（阻塞等刷卡事件）
  std::thread controller([&]{
    NfcEvent ev;
    while (bus.wait_pop(ev)) {
      auto now = std::chrono::steady_clock::now();
      auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - ev.t).count();

      std::cout << "\n[EVENT] NFC UID = " << ev.uid << "\n";
      std::cout << "[LATENCY] " << latency_ms << " ms\n";

      if (is_allowed(ev.uid)) {
        std::cout << "[AUTH] ALLOW -> UNLOCK (simulated)\n";
      } else {
        std::cout << "[AUTH] DENY\n";
      }
      std::cout << "----------------------------------\n";
    }
    std::cout << "Controller stopped.\n";
  });

  // 输入线程：模拟NFC读卡器（键盘输入）
  std::thread input([&]{
    std::cout << "Type UID to simulate NFC swipe.\n";
    std::cout << "Allowed: 1234, A1B2C3, TEST\n";
    std::cout << "Type 'quit' to exit.\n\n";

    std::string uid;
    while (std::cin >> uid) {
      if (uid == "quit") break;
      bus.push(NfcEvent{uid, std::chrono::steady_clock::now()});
    }
    bus.stop();
  });

  input.join();
  controller.join();
  return 0;
}
// 编译：g++ -std=c++17 -pthread simulate_nfc_ test.cpp -o simulate_nfc_test