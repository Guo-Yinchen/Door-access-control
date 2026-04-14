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
  LED    = 1u << 0,// LED 事件目标，表示事件应该被 LED 设备处理 LED event target, indicating the event should be handled by LED devices
  BUZZER = 1u << 1,// 蜂鸣器事件目标，表示事件应该被蜂鸣器设备处理 Buzzer event target, indicating the event should be handled by buzzer devices
  LOCK   = 1u << 2,// 锁事件目标，表示事件应该被锁设备处理 Lock event target, indicating the event should be handled by lock devices
  LOGGER = 1u << 3,// 日志事件目标，表示事件应该被日志系统处理 Logger event target, indicating the event should be handled by logging systems
  ALL    = 0xFFFFFFFFu// 全部设备目标，表示事件应该被所有设备处理 All devices target, indicating the event should be handled by all devices
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

  void subscribe(Target my_target, Handler handler);// subscribe 订阅一个事件处理函数，指定它感兴趣的目标设备 my_target specifies the target devices this handler is interested in
  void publish(AuthResult r, uint32_t targets);// publish 发布一个事件，指定认证结果和目标设备，事件会被分发给所有订阅了相关目标的处理函数 publish an event with a specified authentication result and target devices, which will be dispatched to all handlers subscribed to the relevant targets

  void publish(AuthResult r, Target target) {
    publish(r, static_cast<uint32_t>(target));// 方便的重载版本，接受单个 Target 方便调用 A convenient overload that accepts a single Target for easier use
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