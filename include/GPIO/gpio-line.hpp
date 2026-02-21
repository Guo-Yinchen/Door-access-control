#pragma once
#include <gpiod.h>

class GpioLine {
public:
  GpioLine(const char* chip_name, int line_offset, const char* consumer);
  ~GpioLine();

  GpioLine(const GpioLine&) = delete;
  GpioLine& operator=(const GpioLine&) = delete;

  void set(bool high);
  void on()  { set(true); }
  void off() { set(false); }

private:
  gpiod_line_request* req_ = nullptr;
  unsigned int offset_ = 0;
};