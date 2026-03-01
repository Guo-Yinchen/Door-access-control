#pragma once
#include "GPIO/gpio-line.hpp"
#include "EVENT/event-bus.hpp"   // 你的 EventBus (Target/AuthEvent/AuthResult/EventBus)

#include <chrono>

class StatusLeds {
public:
  StatusLeds(const char* chip_name,
             int red_gpio, int yellow_gpio, int green_gpio,
             const char* consumer = "status_leds");

  void idle();     // 黄灯常亮，红绿灭
  void granted();  // 绿灯亮（黄保持亮），红灭
  void denied();   // 红灯亮（黄保持亮），绿灭
  void all_off();  // 全灭（可选）

  // 接入 EventBus（订阅 GRANTED/DENIED，只订阅 LED 目标）
  // hold_ms：显示 granted/denied 后保持多久，再自动回 idle
  void attach(EventBus& bus, int hold_ms = 1000);

  // 到点自动回 idle
  void tick();

private:
  GpioLine red_;
  GpioLine yellow_;
  GpioLine green_;

  // 显示一段时间后回 idle
  std::chrono::milliseconds hold_{1000};
  bool pending_idle_ = false;
  std::chrono::steady_clock::time_point deadline_{};
};