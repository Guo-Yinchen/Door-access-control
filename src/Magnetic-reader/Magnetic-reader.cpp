#include "Magnetic-reader/Magnetic-reader.hpp"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

MagstripeReader::MagstripeReader() : MagstripeReader(Config{}) {}

MagstripeReader::MagstripeReader(Config cfg) : cfg_(std::move(cfg)) {
  fd_ = ::open(cfg_.device.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("MagstripeReader: open(" + cfg_.device + ") failed: " +
                             std::string(std::strerror(errno)));
  }
}

MagstripeReader::~MagstripeReader() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void MagstripeReader::stop() { stop_ = true; }

char MagstripeReader::keycode_to_char(int code, bool shift) {
  // 数字
  if (code >= KEY_1 && code <= KEY_9) return char('1' + (code - KEY_1));
  if (code == KEY_0) return '0';

  // 字母（一般用不到，但留着）
  if (code >= KEY_A && code <= KEY_Z) {
    char c = char('a' + (code - KEY_A));
    if (shift) c = char(c - 'a' + 'A');
    return c;
  }

  // 常见符号（磁条 Track2 有时会出现 ; 和 =）
  if (!shift) {
    switch (code) {
      case KEY_MINUS:     return '-';
      case KEY_EQUAL:     return '=';
      case KEY_SEMICOLON: return ';';
      case KEY_SLASH:     return '/';
      case KEY_DOT:       return '.';
      case KEY_COMMA:     return ',';
      default: break;
    }
  }

  return 0;
}

void MagstripeReader::run(CardCallback cb) {
  if (!cb) throw std::invalid_argument("MagstripeReader::run: callback is empty");

  bool shift = false;
  std::string buf;

  while (!stop_) {
    input_event ev{};
    const ssize_t n = ::read(fd_, &ev, sizeof(ev));
    if (n < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("MagstripeReader: read failed: " +
                               std::string(std::strerror(errno)));
    }
    if (n != sizeof(ev)) continue;

    if (ev.type != EV_KEY) continue;

    // shift 状态
    if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
      shift = (ev.value != 0);
      continue;
    }

    // 只处理按下（1=press, 0=release, 2=repeat）
    if (ev.value != 1) continue;

    // Enter 结束一条刷卡
    if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
      if (!buf.empty()) {
        cb(buf);
        buf.clear();
      }
      continue;
    }

    // 退格
    if (ev.code == KEY_BACKSPACE) {
      if (!buf.empty()) buf.pop_back();
      continue;
    }

    const char c = keycode_to_char(ev.code, shift);
    if (c) buf.push_back(c);
  }
}