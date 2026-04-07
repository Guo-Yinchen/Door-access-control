#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
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
  std::optional<clock::time_point> t_pending_face;
  std::optional<clock::time_point> t_face_result;
  std::optional<clock::time_point> t_final_feedback;

  static long long ms_between(clock::time_point a, clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
  }

  void log_invalid_denied() const {
    if (!t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=invalid_card_denied"
        << " total_ms=" << ms_between(t_card_read, *t_final_feedback)
        << "\n";
  }

  void log_valid_granted() const {
    if (!t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=valid_card_granted"
        << " total_ms=" << ms_between(t_card_read, *t_final_feedback);

    if (t_card_verified) {
      std::cout << " verify_ms=" << ms_between(t_card_read, *t_card_verified);
    }

    std::cout << "\n";
  }

  void log_pending_face() const {
    if (!t_pending_face) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=high_risk_pending_face"
        << " total_ms=" << ms_between(t_card_read, *t_pending_face);

    if (t_card_verified) {
      std::cout << " verify_ms=" << ms_between(t_card_read, *t_card_verified);
    }

    std::cout << "\n";
  }

  void log_face_final(bool granted) const {
    if (!t_face_result || !t_final_feedback) return;

    std::cout
        << "[LATENCY] trace=" << trace_id
        << " path=" << (granted ? "face_final_granted" : "face_final_denied")
        << " total_from_card_ms=" << ms_between(t_card_read, *t_final_feedback);

    if (t_pending_face) {
      std::cout << " face_stage_ms=" << ms_between(*t_pending_face, *t_face_result);
    }

    if (t_face_result) {
      std::cout << " final_feedback_ms=" << ms_between(*t_face_result, *t_final_feedback);
    }

    std::cout << "\n";
  }
};
