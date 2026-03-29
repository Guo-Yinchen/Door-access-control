#pragma once
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

#include "AuthResult/AuthResult.hpp"

enum class Target : uint32_t {
  LED    = 1u << 0,
  BUZZER = 1u << 1,
  LOCK   = 1u << 2,
  LOGGER = 1u << 3,
  ALL    = 0xFFFFFFFFu
};

inline constexpr uint32_t operator|(Target a, Target b) {
  return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}

inline constexpr uint32_t operator|(uint32_t a, Target b) {
  return a | static_cast<uint32_t>(b);
}

struct AuthEvent {
  AuthResult result;
  uint32_t targets;
};

class EventBus {
public:
  using Handler = std::function<void(const AuthEvent&)>;

  void subscribe(Target my_target, Handler handler);
  void publish(AuthResult r, uint32_t targets);

  void publish(AuthResult r, Target target) {
    publish(r, static_cast<uint32_t>(target));
  }

  void dispatch_loop();
  void stop();

private:
  struct Sub {
    uint32_t target;
    Handler handler;
  };

  std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<AuthEvent> q_;
  std::vector<Sub> subs_;
  bool stop_{false};
};