#include "Magnetic-reader/Magnetic-reader.hpp"

#include <linux/input.h>
#include <fcntl.h>
#include <poll.h>
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

  if (::pipe(stop_pipe_) != 0) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(
      "MagstripeReader: pipe() failed: " + std::string(std::strerror(errno))
    );
  }

  // 让 stop pipe 的写端非阻塞，避免重复 stop 时卡住
  int flags = ::fcntl(stop_pipe_[1], F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(stop_pipe_[1], F_SETFL, flags | O_NONBLOCK);
  }
}

MagstripeReader::~MagstripeReader() {
  stop();

  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (stop_pipe_[0] >= 0) {// Close the stop pipe to clean up resources
    ::close(stop_pipe_[0]);
    stop_pipe_[0] = -1;
  }
  if (stop_pipe_[1] >= 0) {//
    ::close(stop_pipe_[1]);
    stop_pipe_[1] = -1;
  }
}
// stop 设置停止标志并通过管道通知读取循环退出
// stop sets the stop flag and notifies the reading loop to exit via the pipe
void MagstripeReader::stop() {
  const bool already = stop_.exchange(true);
  if (already) return;

  if (stop_pipe_[1] >= 0) {
    const char byte = 'x';
    (void)::write(stop_pipe_[1], &byte, 1);
  }
}
// keycode_to_char 将输入事件的键码转换为对应的字符，考虑 Shift 键的状态
// keycode_to_char converts the keycode of an input event to the corresponding character, considering
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
// run 进入读取循环，调用回调函数处理每条读到的数据，直到接收到停止信号
// run enters the reading loop, calling the callback function to process each read data until a stop
void MagstripeReader::run(CardCallback cb) {
  if (!cb) {
    throw std::invalid_argument("MagstripeReader::run: callback is empty");
  }

  bool shift = false;
  std::string buf;

  while (!stop_.load()) {
    pollfd fds[2];
    fds[0].fd = fd_;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = stop_pipe_[0];
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    const int pr = ::poll(fds, 2, -1);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(
        "MagstripeReader: poll failed: " + std::string(std::strerror(errno))
      );
    }

    if (fds[1].revents & POLLIN) {
      char drain[32];
      (void)::read(stop_pipe_[0], drain, sizeof(drain));
      break;
    }

    if (!(fds[0].revents & POLLIN)) {
      continue;
    }
// 读取输入事件并处理，直到读到回车键表示一条数据结束，或者读到停止信号
// Read input events and process them until an Enter key is read indicating the end of a data entry, or a stop signal is received
    input_event ev{};
    const ssize_t n = ::read(fd_, &ev, sizeof(ev));

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(
        "MagstripeReader: read failed: " + std::string(std::strerror(errno))
      );
    }

    if (n == 0) {
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