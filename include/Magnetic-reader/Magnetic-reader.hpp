#pragma once
#include <atomic>
#include <functional>
#include <string>

class MagstripeReader {
public:
  struct Config {
    std::string device = "/dev/input/by-id/usb-DECETECH.COM.CN_DK_131K-UL_V7.76-event-kbd";
    bool grab_exclusive = false;
  };

  using CardCallback = std::function<void(const std::string&)>;

  MagstripeReader();
  explicit MagstripeReader(Config cfg);
  ~MagstripeReader();

  MagstripeReader(const MagstripeReader&) = delete;
  MagstripeReader& operator=(const MagstripeReader&) = delete;

  void run(CardCallback cb);
  void stop();

private:
  Config cfg_;
  int fd_{-1};
  int stop_pipe_[2]{-1, -1};
  std::atomic<bool> stop_{false};

  static char keycode_to_char(int code, bool shift);
};