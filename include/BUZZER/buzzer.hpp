#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "GPIO/gpio-line.hpp"
#include "EVENT/event-bus.hpp"

class Buzzer {
public:
  Buzzer(const char* chip_name, int buzzer_gpio, const char* consumer = "buzzer");
  ~Buzzer();

  Buzzer(const Buzzer&) = delete;
  Buzzer& operator=(const Buzzer&) = delete;

  void attach(EventBus& bus);

  void all_off();
  void granted();
  void denied();
  void pending_face();

private:
  enum class Mode {
    Off,
    Granted,
    Denied,
    PendingFace
  };

  void worker_loop();

private:
  GpioLine buzzer_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread worker_;
  std::atomic<bool> stop_{false};
  Mode mode_{Mode::Off};
};