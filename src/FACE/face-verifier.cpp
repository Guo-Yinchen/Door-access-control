#include "FACE/face-verifier.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cfloat>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace {
constexpr int kFaceWidth = 160;
constexpr int kFaceHeight = 160;

constexpr int kVerificationWindowMs = 6000;
constexpr int kFrameWaitTimeoutMs = 250;
constexpr int kRequiredMatches = 3;
constexpr int kMaxFramesToCheck = 60;
constexpr double kConfidenceThreshold = 90.0;
//constexpr int kDebugDumpFrames = 6;
}

FaceVerifier::FaceVerifier(CameraStream& camera,
                           const std::string& cascade_path,
                           const std::string& model_path,
                           const std::string& labels_path)
    : camera_(camera),
      ready_(false),
      recognizer_(cv::face::LBPHFaceRecognizer::create(1, 8, 8, 8, DBL_MAX)) {
  bool ok = true;

  char cwd[1024] = {0};
  if (::getcwd(cwd, sizeof(cwd))) {
    std::cout << "[FACE] cwd=" << cwd << "\n";
  }
  std::cout << "[FACE] cascade_path=" << cascade_path << "\n";
  std::cout << "[FACE] model_path=" << model_path << "\n";
  std::cout << "[FACE] labels_path=" << labels_path << "\n";

  if (!face_cascade_.load(cascade_path)) {
    std::cerr << "[FACE] Failed to load cascade: " << cascade_path << "\n";
    ok = false;
  }

  try {
    recognizer_->read(model_path);
  } catch (const cv::Exception& e) {
    std::cerr << "[FACE] Failed to load LBPH model: " << model_path
              << " | " << e.what() << "\n";
    ok = false;
  }

  if (!load_labels(labels_path)) {
    std::cerr << "[FACE] Failed to load labels: " << labels_path << "\n";
    ok = false;
  }

  std::cout << "[FACE] recognizer empty=" << recognizer_->empty() << "\n";
  std::cout << "[FACE] recognizer threshold=" << recognizer_->getThreshold() << "\n";
  std::cout << "[FACE] recognizer histograms=" << recognizer_->getHistograms().size() << "\n";
  std::cout << "[FACE] recognizer labels total=" << recognizer_->getLabels().total() << "\n";
  std::cout << "[FACE] label map size=" << label_to_card_.size() << "\n";

  if (recognizer_->empty() || recognizer_->getHistograms().empty() || recognizer_->getLabels().total() == 0) {
    std::cerr << "[FACE] LBPH model appears empty after loading.\n";
    ok = false;
  }

  ready_ = ok;

  if (ready_) {
    std::cout << "[FACE] Multi-face verifier ready.\n";
  }
}

bool FaceVerifier::is_ready() const {
  return ready_;
}

bool FaceVerifier::load_labels(const std::string& labels_path) {
  std::ifstream fin(labels_path);
  if (!fin.is_open()) {
    return false;
  }

  label_to_card_.clear();

  std::string line;
  while (std::getline(fin, line)) {
    if (line.empty()) {
      continue;
    }

    std::stringstream ss(line);
    std::string label_str;
    std::string card_id;

    if (!std::getline(ss, label_str, ',')) {
      continue;
    }
    if (!std::getline(ss, card_id)) {
      continue;
    }

    try {
      int label = std::stoi(label_str);
      label_to_card_[label] = card_id;
    } catch (...) {
      continue;
    }
  }

  return !label_to_card_.empty();
}

std::vector<cv::Rect> FaceVerifier::detect_faces(const cv::Mat& frame_gray) {
  std::vector<cv::Rect> faces;
  face_cascade_.detectMultiScale(
      frame_gray,
      faces,
      1.1,
      4,
      0,
      cv::Size(60, 60),
      cv::Size());
  return faces;
}
// 取出人脸区域，调整为训练时的大小，使用直方图均衡化增强对比度
cv::Mat FaceVerifier::preprocess_face(const cv::Mat& gray, const cv::Rect& face_rect) const {
  const int pad_x = face_rect.width / 8;
  const int pad_y = face_rect.height / 8;

  cv::Rect padded(
      face_rect.x - pad_x,
      face_rect.y - pad_y,
      face_rect.width + pad_x * 2,
      face_rect.height + pad_y * 2);

  cv::Rect bounded = padded & cv::Rect(0, 0, gray.cols, gray.rows);

  cv::Mat roi = gray(bounded).clone();
  cv::resize(roi, roi, cv::Size(kFaceWidth, kFaceHeight));
  cv::equalizeHist(roi, roi);
  return roi;
}

//统计在验证窗口符合要求的帧数，如果达到要求的匹配数则验证成功，否则失败
//verify the number of frames that meet the requirements in the verification window, if the required number of matches is reached, the verification is successful, otherwise it fails
bool FaceVerifier::verify(const std::string& card_id, const std::atomic<bool>& stop_requested) {
  if (!ready_) {
    std::cerr << "[FACE] Verifier not ready.\n";
    return false;
  }

  if (!camera_.is_running()) {
    std::cerr << "[FACE] Camera stream is not running.\n";
    return false;
  }

  std::cout << "[FACE] Face verification started for card_id: " << card_id << "\n";

  const auto start = std::chrono::steady_clock::now();
  int matched_frames = 0;
  int valid_prediction_frames = 0;
  int dumped_frames = 0;
  double best_confidence = DBL_MAX;

  while (!stop_requested.load()) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    if (elapsed_ms >= kVerificationWindowMs) {
      break;
    }

    cv::Mat frame;
    const bool got_frame = camera_.wait_for_frame(frame, stop_requested, kFrameWaitTimeoutMs);
    if (!got_frame || frame.empty()) {
      continue;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    auto faces = detect_faces(gray);
    if (faces.empty()) {
      continue;
    }

    std::cout << "[FACE] Detected " << faces.size() << " face(s)\n";

    bool matched_this_frame = false;

    for (const auto& face_rect : faces) {
      cv::Mat face_img = preprocess_face(gray, face_rect);
/*测试用 
测试时用于确认预处理是否合理，LBPH模型是否能正确预测
For testing,if the LBPH model can predict correctly
      if (dumped_frames < kDebugDumpFrames) {
        const std::string frame_path = "/tmp/face_debug_frame_" + std::to_string(dumped_frames) + ".png";
        const std::string roi_path = "/tmp/face_debug_roi_" + std::to_string(dumped_frames) + ".png";
        cv::imwrite(frame_path, frame);
        cv::imwrite(roi_path, face_img);
        ++dumped_frames;
      }
*/
      int predicted_label = -1;
      double confidence = DBL_MAX;

      try {
        recognizer_->predict(face_img, predicted_label, confidence);
      } catch (const cv::Exception& e) {
        std::cerr << "[FACE] Predict failed: " << e.what() << "\n";
        continue;
      }

      std::cout << "[FACE] raw predicted_label=" << predicted_label
                << ", confidence=" << confidence << "\n";

      if (predicted_label < 0) {
        continue;
      }

      ++valid_prediction_frames;

      auto it = label_to_card_.find(predicted_label);
      if (it == label_to_card_.end()) {
        continue;
      }

      const std::string& predicted_card = it->second;

      std::cout << "[FACE] predicted_label=" << predicted_label
                << ", mapped_card=" << predicted_card
                << ", confidence=" << confidence << "\n";

      if (predicted_card == card_id && confidence <= kConfidenceThreshold) {
        matched_this_frame = true;
        best_confidence = std::min(best_confidence, confidence);
        break;
      }
    }

    if (matched_this_frame) {
      ++matched_frames;
      std::cout << "[FACE] Match accepted (" << matched_frames
                << "/" << kRequiredMatches << "), best_confidence="
                << best_confidence << "\n";

      if (matched_frames >= kRequiredMatches) {
        std::cout << "[FACE] Face verification PASSED.\n";
        return true;
      }
    }
  }

  std::cout << "[FACE] Face verification FAILED. matched_frames="
            << matched_frames
            << ", best_confidence=" << best_confidence
            << ", valid_prediction_frames=" << valid_prediction_frames
            << "\n";
  return false;
}