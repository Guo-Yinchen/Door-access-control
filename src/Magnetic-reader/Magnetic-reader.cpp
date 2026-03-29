#include "Magnetic-reader/Magnetic-reader.hpp"

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

MagstripeReader::MagstripeReader() : MagstripeReader(Config{}) {}

MagstripeReader::MagstripeReader(Config cfg) : cfg_(std::move(cfg)) {
  fd_ = ::open(cfg_.device.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error(
      "MagstripeReader: open(" + cfg_.device + ") failed: " +
      std::string(std::strerror(errno))
    );
  }
}

MagstripeReader::~MagstripeReader() {
  stop();
}

void MagstripeReader::stop() {
  stop_.store(true);

  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

char MagstripeReader::keycode_to_char(int code, bool shift) {
  if (code >= KEY_1 && code <= KEY_9) return char('1' + (code - KEY_1));
  if (code == KEY_0) return '0';

  if (code >= KEY_A && code <= KEY_Z) {
    char c = char('a' + (code - KEY_A));
    if (shift) c = char(c - 'a' + 'A');
    return c;
  }

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
  if (!cb) {
    throw std::invalid_argument("MagstripeReader::run: callback is empty");
  }

  bool shift = false;
  std::string buf;

  while (!stop_.load()) {
    input_event ev{};
    const ssize_t n = ::read(fd_, &ev, sizeof(ev));

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }

      if (stop_.load() && (errno == EBADF || errno == ENODEV || errno == EIO)) {
        break;
      }

      throw std::runtime_error(
        "MagstripeReader: read failed: " + std::string(std::strerror(errno))
      );
    }

    if (n == 0) {
      if (stop_.load()) {
        break;
      }
      continue;
    }

    if (n != sizeof(ev)) continue;
    if (ev.type != EV_KEY) continue;

    if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
      shift = (ev.value != 0);
      continue;
    }

    if (ev.value != 1) continue;

    if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
      if (!buf.empty()) {
        cb(buf);
        buf.clear();
      }
      continue;
    }

    if (ev.code == KEY_BACKSPACE) {
      if (!buf.empty()) buf.pop_back();
      continue;
    }

    const char c = keycode_to_char(ev.code, shift);
    if (c) buf.push_back(c);
  }
}