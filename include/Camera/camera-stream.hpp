#pragma once

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

class CameraStream {
public:
  struct Config {
    int width{640};
    int height{480};
    int fps{10};
    int read_timeout_ms{200};
    int camera_index{0};
  };

  CameraStream();
  explicit CameraStream(const Config& config);
  ~CameraStream();

  CameraStream(const CameraStream&) = delete;
  CameraStream& operator=(const CameraStream&) = delete;

  bool start();
  void stop();

  bool is_running() const { return running_.load(); }

  bool get_latest_frame(cv::Mat& out) const;
  bool wait_for_frame(cv::Mat& out,
                      const std::atomic<bool>& stop_requested,
                      int timeout_ms = 250);

private:
  void capture_loop();
  bool open_pipe();
  void close_pipe();
  bool read_next_jpeg(std::vector<unsigned char>& jpeg_bytes);

private:
  Config cfg_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  mutable std::mutex frame_mtx_;
  std::condition_variable frame_cv_;
  cv::Mat latest_frame_;
  std::uint64_t frame_seq_{0};

  std::thread worker_;

  FILE* pipe_{nullptr};
  int pipe_fd_{-1};
  std::vector<unsigned char> stream_buffer_;
};