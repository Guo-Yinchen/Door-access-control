#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

#include "AuthResult/AuthResult.hpp"

// 组件标签（以后扩展就在这里加）
enum class Target : uint32_t {
  LED    = 1u << 0,
  BUZZER = 1u << 1,
  LOCK   = 1u << 2,
  LOGGER = 1u << 3,
  ALL    = 0xFFFFFFFFu
};

// 组合 Target 用的 | 运算
inline constexpr uint32_t operator|(Target a, Target b) {
  return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}

struct AuthEvent {
  AuthResult result;   // GRANTED / DENIED / IDLE
  uint32_t targets;    // 发给谁（Target 的组合）
};

class EventBus {
public:
  using Handler = std::function<void(const AuthEvent&)>;

  // 订阅：声明“我属于哪个组件”
  void subscribe(Target my_target, Handler handler);

  // 发布：声明“这次发给哪些组件”
  void publish(AuthResult r, uint32_t targets);

  // 便捷：只发给单个组件（比如现在只发 LED）
  void publish(AuthResult r, Target target) {
    publish(r, static_cast<uint32_t>(target));
  }

  // 处理当前队列中的全部事件（按 targets 过滤）
  void poll();

private:
  struct Sub {
    uint32_t target;
    Handler handler;
  };

  std::mutex mtx_;
  std::queue<AuthEvent> q_;
  std::vector<Sub> subs_;
};