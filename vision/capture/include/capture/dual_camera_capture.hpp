#pragma once

#include "capture/frame.hpp"
#include "capture/v4l2_capture.hpp"
#include <string>

namespace actprove::capture {

struct DualCameraConfig {
  V4L2Config search{};
  V4L2Config tele{"/dev/video1", 60, 1920, 1080, 0, false, "YUYV", 4};
  // If true, failure to open TELE leaves SEARCH usable for the one-camera test.
  bool allow_single_camera{true};
};

/// Two independent UVC streams with a deliberate single-camera fallback.
/// The EO wire cam_id remains 0; CameraRole disambiguates SEARCH and TELE
/// inside the capture/detection process without colliding with THERMAL=1.
class DualCameraCapture {
 public:
  explicit DualCameraCapture(DualCameraConfig cfg);
  ~DualCameraCapture();

  bool open();
  void close();
  bool grab_search(Frame& out);
  bool grab_tele(Frame& out);
  bool tele_available() const { return tele_open_; }

 private:
  DualCameraConfig cfg_;
  V4L2Capture search_;
  V4L2Capture tele_;
  bool search_open_{false};
  bool tele_open_{false};
};

}  // namespace actprove::capture
