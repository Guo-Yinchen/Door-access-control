#pragma once

#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/face.hpp>

#include <atomic>
#include <map>
#include <string>
#include <vector>

class FaceVerifier {
public:
  FaceVerifier(const std::string& cascade_path = "models/haarcascade_frontalface_default.xml",
               const std::string& model_path   = "models/lbph_faces.yml",
               const std::string& labels_path  = "models/face_labels.txt");

  bool verify(const std::string& card_id, const std::atomic<bool>& stop_requested);
  bool is_ready() const;

private:
  bool load_labels(const std::string& labels_path);
  std::vector<cv::Rect> detect_faces(const cv::Mat& frame_gray);
  cv::Mat preprocess_face(const cv::Mat& gray, const cv::Rect& face_rect) const;

private:
  bool ready_;

  cv::CascadeClassifier face_cascade_;
  cv::Ptr<cv::face::LBPHFaceRecognizer> recognizer_;

  std::map<int, std::string> label_to_card_;
};