#pragma once
#include "GPIO/gpio-line.hpp"
#include "EVENT/event-bus.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class StatusLeds {
public:
  StatusLeds(const char* chip_name,
             int red_gpio, int yellow_gpio, int green_gpio,
             const char* consumer = "status_leds");
  ~StatusLeds();

  StatusLeds(const StatusLeds&) = delete;
  StatusLeds& operator=(const StatusLeds&) = delete;

  void idle();     // 黄灯常亮，红绿灭
  void granted();  // 绿灯亮（黄保持亮），红灭
  void denied();   // 红灯亮（黄保持亮），绿灭
  void all_off();  // 全灭（可选）
  void pending_face();  // 红灯+黄灯常亮，绿灭

  // 接入 EventBus，根据事件自动切换状态
  // hold_ms：显示 granted/denied 后保持多久，再自动回 idle
  void attach(EventBus& bus, int hold_ms = 1000);

  // 保留接口，内部由后台线程自动处理延时回 idle
  void tick();

private:
  void start_timer_worker();
  void schedule_idle();
  void cancel_pending_idle();

  GpioLine red_;
  GpioLine yellow_;
  GpioLine green_;

  std::chrono::milliseconds hold_{1000};
  bool pending_idle_ = false;
  bool stop_worker_ = false;
  std::chrono::steady_clock::time_point deadline_{};

  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread timer_thread_;
};