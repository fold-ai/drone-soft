#pragma once
#include "capture/frame.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace actprove::capture {

struct V4L2Config {
  std::string device{"/dev/video0"};
  int fps{60};
  uint16_t width{1920};
  uint16_t height{1080};
  uint8_t cam_id{0};
  bool simulate{false};
  // Uncompressed YUYV is deterministic and avoids a JPEG decoder in the
  // inference hot path. Set "MJPG" explicitly only when USB bandwidth demands it.
  std::string pixel_format{"YUYV"};
  int buffer_count{4};
};

/// USB/lab fallback capture.
/// Ground EO range-test uses this path on the **GCS host** (see
/// `vision/configs/capture_ground_eo_usb.yaml`: `/dev/video0`, 30 fps, cam_id=2).
/// Not the Orin nose GMSL/Argus path — Perception owns live ingest; do not break Argus.
class V4L2Capture {
 public:
  explicit V4L2Capture(V4L2Config cfg);
  ~V4L2Capture();
  bool open();
  void close();
  bool grab(Frame& out);
 private:
  struct MappedBuffer {
    void* start{nullptr};
    size_t length{0};
  };
  V4L2Config cfg_;
  int fd_{-1};
  uint64_t seq_{0};
  std::vector<MappedBuffer> buffers_;
  [[maybe_unused]] PixelFormat negotiated_format_{PixelFormat::Unknown};
  [[maybe_unused]] size_t negotiated_stride_{0};
};

}  // namespace actprove::capture
