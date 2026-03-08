#pragma once

class RiskPolicy {
public:
  // 晚上 22:00 以后要求人脸验证
  bool require_face_now() const;
};