#pragma once
#include <string>

class FaceVerifier {
public:
  // 假的验证接口，实际项目中应该调用人脸识别模块的接口
  bool verify(const std::string& card_id);
};