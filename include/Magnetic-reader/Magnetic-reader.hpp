#pragma once
#include <atomic>
#include <functional>
#include <string>

class MagstripeReader {
public:
  struct Config {
    std::string device = "/dev/input/event9"; // 当前设备 event9
    bool grab_exclusive = false;              // 是否独占抓取(可选)
  };

  using CardCallback = std::function<void(const std::string&)>;

  MagstripeReader();
  explicit MagstripeReader(Config cfg);
  ~MagstripeReader();

  MagstripeReader(const MagstripeReader&) = delete;
  MagstripeReader& operator=(const MagstripeReader&) = delete;

  // 阻塞循环：每刷一次卡(Enter结束)就回调一次
  void run(CardCallback cb);

  // 请求停止循环，安全退出
  void stop();

private:
  Config cfg_;
  int fd_{-1};
  std::atomic<bool> stop_{false};

  static char keycode_to_char(int code, bool shift);
};