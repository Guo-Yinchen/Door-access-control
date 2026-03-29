#include "FACE/face-verifier.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
constexpr int kFaceWidth = 160;
constexpr int kFaceHeight = 160;

// 8 秒窗口
constexpr int kVerificationWindowMs = 8000;

// 每 500ms 拍 1 张 = 2 fps
constexpr int kCaptureIntervalMs = 500;

// 成功 3 次立即通过
constexpr int kRequiredMatches = 3;

// 最多 16 张
constexpr int kMaxCaptures = 16;

// LBPH 阈值，越小越严格
constexpr double kConfidenceThreshold = 65.0;

// 临时图片路径
const char* kCapturePath = "/tmp/face_verify.jpg";
}

FaceVerifier::FaceVerifier(const std::string& cascade_path,
                           const std::string& model_path,
                           const std::string& labels_path)
    : ready_(false),
      recognizer_(cv::face::LBPHFaceRecognizer::create(1, 8, 8, 8, kConfidenceThreshold)) {
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

bool FaceVerifier::verify(const std::string& card_id) {
  if (!ready_) {
    std::cerr << "[FACE] Verifier not ready.\n";
    return false;
  }

  std::cout << "[FACE] Face verification started for card_id: " << card_id << "\n";

  const auto start = std::chrono::steady_clock::now();
  int matched_frames = 0;
  int capture_count = 0;

  while (capture_count < kMaxCaptures) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    if (elapsed_ms >= kVerificationWindowMs) {
      break;
    }

    // 删除旧图，避免误读
    std::remove(kCapturePath);

    // 用 rpicam-still 拍一张
    // -n: no preview
    // -t 300: 快速拍照
    // --immediate: 尽快完成
    const std::string cmd =
        "rpicam-still -n --immediate -t 300 -o " + std::string(kCapturePath) + " >/dev/null 2>&1";

    const int ret = std::system(cmd.c_str());
    ++capture_count;

    if (ret != 0) {
      std::cerr << "[FACE] Capture command failed at frame " << capture_count << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(kCaptureIntervalMs));
      continue;
    }

    cv::Mat frame = cv::imread(kCapturePath, cv::IMREAD_COLOR);
    if (frame.empty()) {
      std::cerr << "[FACE] Failed to read captured image at frame " << capture_count << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(kCaptureIntervalMs));
      continue;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    auto faces = detect_faces(gray);

    if (faces.empty()) {
      std::cout << "[FACE] No face detected in capture " << capture_count << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(kCaptureIntervalMs));
      continue;
    }

    std::cout << "[FACE] Detected " << faces.size()
              << " face(s) in capture " << capture_count << "\n";

    bool matched_this_capture = false;

    for (const auto& face_rect : faces) {
      cv::Mat face_img = preprocess_face(gray, face_rect);

      int predicted_label = -1;
      double confidence = 9999.0;

      try {
        recognizer_->predict(face_img, predicted_label, confidence);
      } catch (const cv::Exception& e) {
        std::cerr << "[FACE] Predict failed: " << e.what() << "\n";
        continue;
      }

      auto it = label_to_card_.find(predicted_label);
      if (it == label_to_card_.end()) {
        continue;
      }

      const std::string& predicted_card = it->second;

      std::cout << "[FACE] predicted_label=" << predicted_label
                << ", mapped_card=" << predicted_card
                << ", confidence=" << confidence << "\n";

      if (predicted_card == card_id && confidence <= kConfidenceThreshold) {
        matched_this_capture = true;
        break;
      }
    }

    if (matched_this_capture) {
      ++matched_frames;
      std::cout << "[FACE] Match accepted (" << matched_frames
                << "/" << kRequiredMatches << ")\n";

      if (matched_frames >= kRequiredMatches) {
        std::cout << "[FACE] Face verification PASSED.\n";
        return true;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kCaptureIntervalMs));
  }

  std::cout << "[FACE] Face verification FAILED.\n";
  return false;
}