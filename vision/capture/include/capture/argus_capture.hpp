#pragma once
#include "capture/frame.hpp"
#include <memory>
#include <string>

namespace actprove::capture {

struct ArgusConfig {
  std::string sensor_mode{"imx900"};
  int fps{60};
  float exposure_us{300.0f};
  uint8_t cam_id{0};
  bool simulate{false};
  // USB cameras do not use Argus. When set, this facade opens the device via
  // V4L2, allowing the same node to run on the selected ELP USB3 hardware.
  std::string v4l2_fallback_device{};
  uint16_t width{1920};
  uint16_t height{1080};
};

/// Primary CSI path via libargus (Jetson Multimedia API) — NVMM zero-copy.
class ArgusCapture {
 public:
  explicit ArgusCapture(ArgusConfig cfg);
  ~ArgusCapture();
  bool open();
  void close();
  /// Blocking grab; stamps Frame.t_pps = mid-exposure.
  bool grab(Frame& out);
  uint64_t dropped() const { return dropped_; }
 private:
  class Impl;
  ArgusConfig cfg_;
  std::unique_ptr<Impl> impl_;
  bool open_{false};
  uint64_t seq_{0};
  uint64_t dropped_{0};
};

}  // namespace actprove::capture
