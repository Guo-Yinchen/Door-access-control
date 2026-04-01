// Uses libgpiod (Linux GPIO character device) v2 C API.
// See: https://libgpiod.readthedocs.io/
#include "GPIO/gpio-line.hpp"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

static std::string chip_path_from_name(const char* chip_name) {
  if (!chip_name) return "/dev/gpiochip0";
  std::string s(chip_name);
  if (s.rfind("/dev/", 0) == 0) return s;       // already path
  return std::string("/dev/") + s;              // "gpiochip0" -> "/dev/gpiochip0"
}

static gpiod_line_request* request_output_line(const char* chip_name,
                                               unsigned int offset,
                                               const char* consumer,
                                               enum gpiod_line_value initial) {
  const std::string chip_path = chip_path_from_name(chip_name);

  gpiod_chip* chip = gpiod_chip_open(chip_path.c_str());
  if (!chip) return nullptr;

  gpiod_line_settings* settings = gpiod_line_settings_new();
  if (!settings) { gpiod_chip_close(chip); return nullptr; }
  gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

  gpiod_line_config* line_cfg = gpiod_line_config_new();
  if (!line_cfg) {
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
    return nullptr;
  }

  int ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
  if (ret) {
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
    return nullptr;
  }

  // initial output value
  ret = gpiod_line_config_set_output_values(line_cfg, &initial, 1);
  if (ret) {
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
    return nullptr;
  }

  gpiod_request_config* req_cfg = gpiod_request_config_new();
  if (req_cfg && consumer) gpiod_request_config_set_consumer(req_cfg, consumer);

  gpiod_line_request* request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

  gpiod_request_config_free(req_cfg);
  gpiod_line_config_free(line_cfg);
  gpiod_line_settings_free(settings);
  gpiod_chip_close(chip);

  return request;
}

GpioLine::GpioLine(const char* chip_name, int line_offset, const char* consumer)
  : offset_(static_cast<unsigned int>(line_offset)) {
  req_ = request_output_line(chip_name, offset_, consumer ? consumer : "door-access", GPIOD_LINE_VALUE_INACTIVE);
  if (!req_) {
    throw std::runtime_error(std::string("Failed to request GPIO line: ") + std::strerror(errno));
  }
}

GpioLine::~GpioLine() {
  if (req_) {
    // best-effort: set low then release
    gpiod_line_request_set_value(req_, offset_, GPIOD_LINE_VALUE_ACTIVE);
    gpiod_line_request_release(req_);
    req_ = nullptr;
  }
}

void GpioLine::set(bool high) {
  if (!req_) return;
  gpiod_line_request_set_value(
      req_, offset_,
      high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE
  );
}
