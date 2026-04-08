#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

struct AuthTrace {
  using clock = std::chrono::steady_clock;

  uint64_t trace_id{0};
  std::string card_id;
  bool card_valid{false};
  bool high_risk{false};

  clock::time_point t_card_read{};
  std::optional<clock::time_point> t_card_verified;
  std::optional<clock::time_point> t_pending_face;      // 决定进入人脸流程
  std::optional<clock::time_point> t_face_task_taken;   // 人脸线程真正开始处理
  std::optional<clock::time_point> t_face_result;       // 人脸识别完成
  std::optional<clock::time_point> t_final_feedback;    // 最终反馈发出

  static long long ms_between(clock::time_point a, clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
  }

  void log_invalid_denied() const {
    if (!t_card_verified || !t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=invalid_card_denied"
        << " card_verify_ms=" << ms_between(t_card_read, *t_card_verified)
        << " verify_to_feedback_ms=" << ms_between(*t_card_verified, *t_final_feedback)
        << " total_ms=" << ms_between(t_card_read, *t_final_feedback)
        << "\n";
  }

  void log_valid_granted() const {
    if (!t_card_verified || !t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=valid_card_granted"
        << " card_verify_ms=" << ms_between(t_card_read, *t_card_verified)
        << " verify_to_feedback_ms=" << ms_between(*t_card_verified, *t_final_feedback)
        << " total_ms=" << ms_between(t_card_read, *t_final_feedback)
        << "\n";
  }

  void log_pending_face() const {
    if (!t_card_verified || !t_pending_face) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=high_risk_pending_face"
        << " card_verify_ms=" << ms_between(t_card_read, *t_card_verified)
        << " verify_to_pending_ms=" << ms_between(*t_card_verified, *t_pending_face)
        << " total_to_pending_ms=" << ms_between(t_card_read, *t_pending_face)
        << "\n";
  }

  void log_face_final(bool granted) const {
    if (!t_card_verified || !t_pending_face || !t_face_result || !t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=" << (granted ? "face_final_granted" : "face_final_denied")
        << " card_verify_ms=" << ms_between(t_card_read, *t_card_verified)
        << " verify_to_pending_ms=" << ms_between(*t_card_verified, *t_pending_face);

    if (t_face_task_taken) {
      std::cout
          << " pending_queue_ms=" << ms_between(*t_pending_face, *t_face_task_taken)
          << " face_verify_ms=" << ms_between(*t_face_task_taken, *t_face_result);
    } else {
      std::cout
          << " pending_to_face_result_ms=" << ms_between(*t_pending_face, *t_face_result);
    }

    std::cout
        << " face_result_to_feedback_ms=" << ms_between(*t_face_result, *t_final_feedback)
        << " total_ms=" << ms_between(t_card_read, *t_final_feedback)
        << "\n";
  }
};