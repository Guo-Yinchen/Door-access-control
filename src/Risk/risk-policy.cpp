#include "Risk/risk-policy.hpp"

#include <ctime>

bool RiskPolicy::require_face_now() const {
  std::time_t now = std::time(nullptr);
  std::tm* local = std::localtime(&now);
  if (!local) return false;

  // 22:00 以后视为高危时段
  return local->tm_hour >= 22;
}