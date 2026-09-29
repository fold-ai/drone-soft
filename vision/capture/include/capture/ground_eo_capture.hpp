#pragma once
#include "capture/frame.hpp"
#include "capture/v4l2_capture.hpp"
#include <string>

namespace actprove::capture {

/// GCS laptop USB/V4L2 Ground EO (not Orin nose GMSL).
struct GroundEoConfig {
  std::string device{"/dev/video0"};
  int fps{30};
  uint16_t width{1920};
  uint16_t height{1080};
  uint8_t cam_id{2};  // CamId::GroundEo on DetectionMsg wire
  bool simulate{false};
};

/// Wraps V4L2; sets cam_id=2 and source_role=ground_eo on Frame.
class GroundEoCapture {
 public:
  explicit GroundEoCapture(GroundEoConfig cfg);
  ~GroundEoCapture();
  bool open();
  void close();
  bool grab(Frame& out);

 private:
  GroundEoConfig cfg_;
  V4L2Capture inner_;
};

}  // namespace actprove::capture
