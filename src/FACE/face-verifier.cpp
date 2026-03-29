#include "FACE/face-verifier.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
constexpr int kFaceWidth  = 160;
constexpr int kFaceHeight = 160;
constexpr int kCaptureFrames = 20;         // 最多看 20 帧
constexpr int kRequiredMatches = 2;        // 至少 2 次匹配成功，降低误判
constexpr double kConfidenceThreshold = 65.0; // LBPH 距离阈值，越小越严格
}

FaceVerifier::FaceVerifier(const std::string& cascade_path,
                           const std::string& model_path,
                           const std::string& labels_path,
                           int camera_index)
    : ready_(false),
      camera_index_(camera_index),
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

  // 文件格式示例：
  // 1,alice_card
  // 2,bob_card
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

std::vector<cv::Rect> FaceVerifier::detect_faces(const cv::Mat& frame_gray) const {
  std::vector<cv::Rect> faces;
  face_cascade_.detectMultiScale(
      frame_gray,
      faces,
      1.1,              // scaleFactor
      4,                // minNeighbors
      0,
      cv::Size(60, 60), // minSize
      cv::Size()        // maxSize
  );
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

  cv::VideoCapture cap(camera_index_);
  if (!cap.isOpened()) {
    std::cerr << "[FACE] Failed to open camera index " << camera_index_ << "\n";
    return false;
  }

  std::cout << "[FACE] Face verification started for card_id: " << card_id << "\n";

  int matched_frames = 0;

  for (int i = 0; i < kCaptureFrames; ++i) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      continue;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    auto faces = detect_faces(gray);

    if (faces.empty()) {
      std::cout << "[FACE] No face detected in frame " << i + 1 << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    std::cout << "[FACE] Detected " << faces.size() << " face(s) in frame " << i + 1 << "\n";

    // 多人脸：逐个识别
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

      // 注意：LBPH 的 confidence 本质上是距离，通常越小越像
      if (predicted_card == card_id && confidence <= kConfidenceThreshold) {
        ++matched_frames;
        std::cout << "[FACE] Match accepted (" << matched_frames
                  << "/" << kRequiredMatches << ")\n";

        if (matched_frames >= kRequiredMatches) {
          std::cout << "[FACE] Face verification PASSED.\n";
          return true;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "[FACE] Face verification FAILED.\n";
  return false;
}