#pragma once
#include "capture/frame.hpp"
#include "detect/trt_engine.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace actprove::detect {

struct Box {
  float x{0}, y{0}, w{0}, h{0};
  float conf{0};
  uint32_t class_id{0};
};

static constexpr uint32_t MAX_DET = 32;

struct DetectionBatch {
  double t_pps{0};
  uint64_t seq{0};
  uint8_t cam_id{0};
  uint16_t src_w{0};
  uint16_t src_h{0};
  uint32_t n{0};
  Box boxes[MAX_DET]{};
};

class Detector {
 public:
  explicit Detector(std::string engine_path, bool simulate = false);
  bool load();
  bool infer(const capture::Frame& in, DetectionBatch& out);
 private:
  std::string engine_path_;
  std::unique_ptr<TrtEngine> engine_;
  bool simulate_{false};
  bool ready_{false};
  float conf_thresh_{0.25f};
  float nms_iou_{0.45f};
};

}  // namespace actprove::detect
