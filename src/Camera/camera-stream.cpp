#include "Camera/camera-stream.hpp"

#include <opencv2/imgcodecs.hpp>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <unistd.h>

namespace {
std::size_t find_marker(const std::vector<unsigned char>& buf,
                        std::size_t from,
                        unsigned char a,
                        unsigned char b) {
  if (buf.size() < 2 || from >= buf.size() - 1) {
    return std::string::npos;
  }

  for (std::size_t i = from; i + 1 < buf.size(); ++i) {
    if (buf[i] == a && buf[i + 1] == b) {
      return i;
    }
  }
  return std::string::npos;
}
} // namespace

CameraStream::CameraStream()
    : cfg_{} {}

CameraStream::CameraStream(const Config& config)
    : cfg_(config) {}

CameraStream::~CameraStream() {
  stop();
}

bool CameraStream::start() {
  if (running_.load()) {
    return true;
  }

  if (!open_pipe()) {
    return false;
  }

  stop_requested_.store(false);
  running_.store(true);

  worker_ = std::thread([this]() {
    capture_loop();
  });

  return true;
}

void CameraStream::stop() {
  stop_requested_.store(true);
  frame_cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  close_pipe();
  running_.store(false);
}

bool CameraStream::get_latest_frame(cv::Mat& out) const {
  std::lock_guard<std::mutex> lock(frame_mtx_);
  if (latest_frame_.empty()) {
    return false;
  }
  out = latest_frame_.clone();
  return true;
}

bool CameraStream::wait_for_frame(cv::Mat& out,
                                  const std::atomic<bool>& external_stop,
                                  int timeout_ms) {
  std::unique_lock<std::mutex> lock(frame_mtx_);
  const std::uint64_t old_seq = frame_seq_;

  const bool woke = frame_cv_.wait_for(
      lock,
      std::chrono::milliseconds(timeout_ms),
      [&]() {
        return stop_requested_.load() || external_stop.load() || frame_seq_ != old_seq;
      });

  if (!woke) {
    return false;
  }

  if ((stop_requested_.load() || external_stop.load()) && frame_seq_ == old_seq) {
    return false;
  }

  if (latest_frame_.empty()) {
    return false;
  }

  out = latest_frame_.clone();
  return true;
}

bool CameraStream::open_pipe() {
  if (pipe_) {
    return true;
  }

  std::ostringstream cmd;
  cmd << "rpicam-vid"
      << " --camera " << cfg_.camera_index
      << " -n"
      << " -t 0"
      << " --width " << cfg_.width
      << " --height " << cfg_.height
      << " --framerate " << cfg_.fps
      << " --codec mjpeg"
      << " -o -"
      << " 2>/dev/null";

  pipe_ = ::popen(cmd.str().c_str(), "r");
  if (!pipe_) {
    std::cerr << "[CAM] Failed to start rpicam-vid process.\n";
    return false;
  }

  pipe_fd_ = ::fileno(pipe_);
  if (pipe_fd_ < 0) {
    std::cerr << "[CAM] Failed to get pipe fd.\n";
    close_pipe();
    return false;
  }

  const int flags = ::fcntl(pipe_fd_, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(pipe_fd_, F_SETFL, flags | O_NONBLOCK);
  }

  stream_buffer_.clear();
  return true;
}

void CameraStream::close_pipe() {
  if (pipe_) {
    ::pclose(pipe_);
    pipe_ = nullptr;
  }
  pipe_fd_ = -1;
  stream_buffer_.clear();
}

void CameraStream::capture_loop() {
  std::cout << "[CAM] CSI camera stream started.\n";

  while (!stop_requested_.load()) {
    std::vector<unsigned char> jpeg_bytes;
    if (!read_next_jpeg(jpeg_bytes)) {
      if (stop_requested_.load()) {
        break;
      }

      std::cerr << "[CAM] Camera stream read failed or ended.\n";
      break;
    }

    cv::Mat encoded(1, static_cast<int>(jpeg_bytes.size()), CV_8UC1, jpeg_bytes.data());
    cv::Mat frame = cv::imdecode(encoded, cv::IMREAD_COLOR);

    if (frame.empty()) {
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(frame_mtx_);
      latest_frame_ = frame;
      ++frame_seq_;
    }

    frame_cv_.notify_all();
  }

  running_.store(false);
  frame_cv_.notify_all();
  std::cout << "[CAM] CSI camera stream stopped.\n";
}

bool CameraStream::read_next_jpeg(std::vector<unsigned char>& jpeg_bytes) {
  if (!pipe_ || pipe_fd_ < 0) {
    return false;
  }

  constexpr std::size_t kChunkSize = 4096;
  unsigned char chunk[kChunkSize];

  while (!stop_requested_.load()) {
    std::size_t soi = find_marker(stream_buffer_, 0, 0xFF, 0xD8);
    if (soi != std::string::npos && soi > 0) {
      stream_buffer_.erase(stream_buffer_.begin(),
                           stream_buffer_.begin() + static_cast<std::ptrdiff_t>(soi));
      soi = 0;
    } else if (soi == std::string::npos && stream_buffer_.size() > 1) {
      unsigned char last = stream_buffer_.back();
      stream_buffer_.clear();
      if (last == 0xFF) {
        stream_buffer_.push_back(last);
      }
    }

    if (!stream_buffer_.empty()) {
      soi = find_marker(stream_buffer_, 0, 0xFF, 0xD8);
      if (soi != std::string::npos) {
        std::size_t eoi = find_marker(stream_buffer_, soi + 2, 0xFF, 0xD9);
        if (eoi != std::string::npos) {
          const std::size_t end_pos = eoi + 2;
          jpeg_bytes.assign(stream_buffer_.begin(),
                            stream_buffer_.begin() + static_cast<std::ptrdiff_t>(end_pos));
          stream_buffer_.erase(stream_buffer_.begin(),
                               stream_buffer_.begin() + static_cast<std::ptrdiff_t>(end_pos));
          return true;
        }
      }
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pipe_fd_, &rfds);

    timeval tv{};
    tv.tv_sec = cfg_.read_timeout_ms / 1000;
    tv.tv_usec = (cfg_.read_timeout_ms % 1000) * 1000;

    const int ready = ::select(pipe_fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "[CAM] select() failed: " << std::strerror(errno) << "\n";
      return false;
    }

    if (ready == 0) {
      continue;
    }

    const ssize_t n = ::read(pipe_fd_, chunk, sizeof(chunk));
    if (n > 0) {
      stream_buffer_.insert(stream_buffer_.end(), chunk, chunk + n);

      if (stream_buffer_.size() > 8 * 1024 * 1024) {
        std::cerr << "[CAM] Stream buffer overflow protection triggered.\n";
        stream_buffer_.clear();
      }
      continue;
    }

    if (n == 0) {
      return false;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      continue;
    }

    std::cerr << "[CAM] read() failed: " << std::strerror(errno) << "\n";
    return false;
  }

  return false;
}