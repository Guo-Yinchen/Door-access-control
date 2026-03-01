#include "LED/led-v1.hpp"   
#include <chrono>

StatusLeds::StatusLeds(const char* chip_name,
                       int red_gpio, int yellow_gpio, int green_gpio,
                       const char* consumer)
  : red_(chip_name, red_gpio, consumer),
    yellow_(chip_name, yellow_gpio, consumer),
    green_(chip_name, green_gpio, consumer) {}

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

// ===================== ✅ 新增：接入 EventBus =====================
void StatusLeds::attach(EventBus& bus, int hold_ms) {
  hold_ = std::chrono::milliseconds(hold_ms);

  // 默认状态
  idle();

  // 只订阅发给 LED 的事件
  bus.subscribe(Target::LED, [this](const AuthEvent& e) {
    if (e.result == AuthResult::granted) granted();
    else                                 denied();

    pending_idle_ = true;
    deadline_ = std::chrono::steady_clock::now() + hold_;
  });
}

// ===================== ✅ 新增：到点回 idle（不阻塞） =====================
void StatusLeds::tick() {
  if (!pending_idle_) return;
  if (std::chrono::steady_clock::now() >= deadline_) {
    idle();
    pending_idle_ = false;
  }
}