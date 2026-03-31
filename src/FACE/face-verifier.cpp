#include "FACE/face-verifier.hpp"

#include <opencv2/imgproc.hpp>

#include <cfloat>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
constexpr int kFaceWidth = 160;
constexpr int kFaceHeight = 160;

constexpr int kVerificationWindowMs = 8000;
constexpr int kFrameWaitTimeoutMs = 250;
constexpr int kRequiredMatches = 3;
constexpr int kMaxFramesToCheck = 60;
constexpr double kConfidenceThreshold = 90.0;
} // namespace

FaceVerifier::FaceVerifier(CameraStream& camera,
                           const std::string& cascade_path,
                           const std::string& model_path,
                           const std::string& labels_path)
    : camera_(camera),
      ready_(false),
      recognizer_(cv::face::LBPHFaceRecognizer::create(1, 8, 8, 8, DBL_MAX)) {
  bool ok = true;

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

cv::Mat FaceVerifier::preprocess_face(const cv::Mat& gray, const cv::Rect& face_rect) const {
  cv::Mat roi = gray(face_rect).clone();
  cv::resize(roi, roi, cv::Size(kFaceWidth, kFaceHeight));
  cv::equalizeHist(roi, roi);
  return roi;
}

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
  int checked_frames = 0;

  while (checked_frames < kMaxFramesToCheck) {
    if (stop_requested.load()) {
      std::cout << "[FACE] Face verification aborted by stop request.\n";
      return false;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    if (elapsed_ms >= kVerificationWindowMs) {
      break;
    }

    cv::Mat frame;
    const bool got_frame = camera_.wait_for_frame(frame, stop_requested, kFrameWaitTimeoutMs);
    if (!got_frame) {
      continue;
    }

    if (frame.empty()) {
      continue;
    }

    ++checked_frames;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    auto faces = detect_faces(gray);

    if (faces.empty()) {
      std::cout << "[FACE] No face detected in frame " << checked_frames << "\n";
      continue;
    }

    std::cout << "[FACE] Detected " << faces.size()
              << " face(s) in frame " << checked_frames << "\n";

    bool matched_this_frame = false;

    for (const auto& face_rect : faces) {
      if (stop_requested.load()) {
        std::cout << "[FACE] Face verification aborted by stop request.\n";
        return false;
      }

      cv::Mat face_img = preprocess_face(gray, face_rect);

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
        std::cout << "[FACE] recognizer returned unknown label\n";
        continue;
      }

      auto it = label_to_card_.find(predicted_label);
      if (it == label_to_card_.end()) {
        std::cout << "[FACE] predicted label not found in label map\n";
        continue;
      }

      const std::string& predicted_card = it->second;

      std::cout << "[FACE] predicted_label=" << predicted_label
                << ", mapped_card=" << predicted_card
                << ", confidence=" << confidence << "\n";

      if (predicted_card == card_id && confidence <= kConfidenceThreshold) {
        matched_this_frame = true;
        break;
      }
    }

    if (matched_this_frame) {
      ++matched_frames;
      std::cout << "[FACE] Match accepted (" << matched_frames
                << "/" << kRequiredMatches << ")\n";

      if (matched_frames >= kRequiredMatches) {
        std::cout << "[FACE] Face verification PASSED.\n";
        return true;
      }
    }
  }

  std::cout << "[FACE] Face verification FAILED.\n";
  return false;
}