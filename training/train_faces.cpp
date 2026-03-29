#include <opencv2/core.hpp>
#include <opencv2/face.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr int kFaceWidth = 160;
constexpr int kFaceHeight = 160;
constexpr double kThreshold = 65.0;

bool is_image_file(const fs::path& p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

cv::Mat preprocess_face(const cv::Mat& gray, const cv::Rect& r) {
  cv::Mat face = gray(r).clone();
  cv::resize(face, face, cv::Size(kFaceWidth, kFaceHeight));
  cv::equalizeHist(face, face);
  return face;
}

bool detect_largest_face(const cv::Mat& gray,
                         cv::CascadeClassifier& cascade,
                         cv::Rect& best_face) {
  std::vector<cv::Rect> faces;
  cascade.detectMultiScale(
      gray,
      faces,
      1.1,
      4,
      0,
      cv::Size(60, 60),
      cv::Size()
  );

  if (faces.empty()) {
    return false;
  }

  best_face = *std::max_element(
      faces.begin(), faces.end(),
      [](const cv::Rect& a, const cv::Rect& b) {
        return a.area() < b.area();
      });

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr
        << "Usage:\n"
        << "  " << argv[0]
        << " <dataset_dir> <cascade_xml> <output_model_yml> <output_labels_txt>\n\n"
        << "Example:\n"
        << "  " << argv[0]
        << " dataset models/haarcascade_frontalface_default.xml "
        << "models/lbph_faces.yml models/face_labels.txt\n";
    return 1;
  }

  const fs::path dataset_dir = argv[1];
  const std::string cascade_path = argv[2];
  const std::string output_model = argv[3];
  const std::string output_labels = argv[4];

  if (!fs::exists(dataset_dir) || !fs::is_directory(dataset_dir)) {
    std::cerr << "[TRAIN] Dataset directory not found: " << dataset_dir << "\n";
    return 1;
  }

  cv::CascadeClassifier face_cascade;
  if (!face_cascade.load(cascade_path)) {
    std::cerr << "[TRAIN] Failed to load cascade: " << cascade_path << "\n";
    return 1;
  }

  std::vector<cv::Mat> images;
  std::vector<int> labels;

  std::map<int, std::string> label_to_card;
  int next_label = 1;

  for (const auto& person_dir : fs::directory_iterator(dataset_dir)) {
    if (!person_dir.is_directory()) {
      continue;
    }

    const std::string card_id = person_dir.path().filename().string();
    const int label = next_label++;
    label_to_card[label] = card_id;

    int loaded_for_person = 0;

    for (const auto& img_file : fs::directory_iterator(person_dir.path())) {
      if (!img_file.is_regular_file() || !is_image_file(img_file.path())) {
        continue;
      }

      cv::Mat img = cv::imread(img_file.path().string(), cv::IMREAD_COLOR);
      if (img.empty()) {
        std::cerr << "[TRAIN] Failed to read image: " << img_file.path() << "\n";
        continue;
      }

      cv::Mat gray;
      cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

      cv::Rect face_rect;
      if (!detect_largest_face(gray, face_cascade, face_rect)) {
        std::cerr << "[TRAIN] No face detected: " << img_file.path() << "\n";
        continue;
      }

      cv::Mat face = preprocess_face(gray, face_rect);
      images.push_back(face);
      labels.push_back(label);
      ++loaded_for_person;

      std::cout << "[TRAIN] Loaded face for " << card_id
                << " from " << img_file.path().filename().string() << "\n";
    }

    std::cout << "[TRAIN] Person " << card_id
              << " -> label " << label
              << ", samples: " << loaded_for_person << "\n";
  }

  if (images.empty()) {
    std::cerr << "[TRAIN] No valid face samples found.\n";
    return 1;
  }

  auto recognizer = cv::face::LBPHFaceRecognizer::create(
      1,   // radius
      8,   // neighbors
      8,   // grid_x
      8,   // grid_y
      kThreshold
  );

  recognizer->train(images, labels);
  recognizer->write(output_model);

  std::ofstream fout(output_labels);
  if (!fout.is_open()) {
    std::cerr << "[TRAIN] Failed to write label file: " << output_labels << "\n";
    return 1;
  }

  for (const auto& [label, card_id] : label_to_card) {
    fout << label << "," << card_id << "\n";
  }

  std::cout << "[TRAIN] Model saved to: " << output_model << "\n";
  std::cout << "[TRAIN] Labels saved to: " << output_labels << "\n";
  std::cout << "[TRAIN] Total training samples: " << images.size() << "\n";

  return 0;
}