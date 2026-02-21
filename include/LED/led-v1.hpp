#pragma once
#include "GPIO/gpio-line.hpp"

class StatusLeds {
public:
  StatusLeds(const char* chip_name,
             int red_gpio, int yellow_gpio, int green_gpio,
             const char* consumer = "status_leds");

  void idle();     // 黄灯常亮，红绿灭
  void granted();  // 绿灯亮（黄保持亮），红灭
  void denied();   // 红灯亮（黄保持亮），绿灭
  void all_off();  // 全灭（可选）

private:
  GpioLine red_;
  GpioLine yellow_;
  GpioLine green_;
};