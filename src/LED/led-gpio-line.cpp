#include "GPIO/gpio-line.hpp"
#include <stdexcept>

GpioLine::GpioLine(const char* chip_name, int line_offset, const char* consumer) {
  chip_ = gpiod_chip_open_by_name(chip_name);
  if (!chip_) {
    throw std::runtime_error("gpiod_chip_open_by_name failed");
  }

  line_ = gpiod_chip_get_line(chip_, line_offset);
  if (!line_) {
    gpiod_chip_close(chip_);
    chip_ = nullptr;
    throw std::runtime_error("gpiod_chip_get_line failed");
  }

  // 申请输出模式，初始值 0（灯灭）
  if (gpiod_line_request_output(line_, consumer, 0) < 0) {
    gpiod_chip_close(chip_);
    chip_ = nullptr;
    line_ = nullptr;
    throw std::runtime_error("gpiod_line_request_output failed (permission or busy?)");
  }
}

GpioLine::~GpioLine() {
  if (line_) {
    gpiod_line_set_value(line_, 0);
    gpiod_line_release(line_);
    line_ = nullptr;
  }
  if (chip_) {
    gpiod_chip_close(chip_);
    chip_ = nullptr;
  }
}

void GpioLine::set(bool high) {
  if (!line_) return;
  gpiod_line_set_value(line_, high ? 1 : 0);
}