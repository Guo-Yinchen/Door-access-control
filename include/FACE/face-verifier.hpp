#pragma once

#include "Camera/camera-stream.hpp"

#include <opencv2/core.hpp>
#include <opencv2/face.hpp>
#include <opencv2/objdetect.hpp>

#include <atomic>
#include <map>
#include <string>
#include <vector>

class FaceVerifier {
public:
  FaceVerifier(CameraStream& camera,
               const std::string& cascade_path = "models/haarcascade_frontalface_default.xml",// 人脸检测级联分类器路径，默认为 OpenCV 自带的 frontalface 分类器路径 by default uses OpenCV's built-in frontalface classifier
               const std::string& model_path   = "models/lbph_faces.yml",// 人脸识别模型路径，默认为训练好的 LBPH 模型路径 by default uses a trained LBPH model path
               const std::string& labels_path  = "models/face_labels.txt");// 标签文件路径，默认为标签文件路径 by default uses a label file path

  bool verify(const std::string& card_id, const std::atomic<bool>& stop_requested);
  bool is_ready() const;

private:
  bool load_labels(const std::string& labels_path);// 加载标签文件，建立标签到卡 ID 的映射 Load label file and build mapping from label to card ID
  std::vector<cv::Rect> detect_faces(const cv::Mat& frame_gray);
  cv::Mat preprocess_face(const cv::Mat& gray, const cv::Rect& face_rect) const;

private:
  CameraStream& camera_;
  bool ready_;

  cv::CascadeClassifier face_cascade_;
  cv::Ptr<cv::face::LBPHFaceRecognizer> recognizer_;

  std::map<int, std::string> label_to_card_;
};