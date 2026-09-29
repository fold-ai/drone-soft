#include "capture/ground_eo_capture.hpp"

namespace actprove::capture {

GroundEoCapture::GroundEoCapture(GroundEoConfig cfg)
    : cfg_(cfg),
      inner_(V4L2Config{
          cfg.device,
          cfg.fps,
          cfg.width,
          cfg.height,
          cfg.cam_id,  // 2 = GroundEo
          cfg.simulate,
          "YUYV",
          4,
      }) {}

GroundEoCapture::~GroundEoCapture() { close(); }

bool GroundEoCapture::open() { return inner_.open(); }

void GroundEoCapture::close() { inner_.close(); }

bool GroundEoCapture::grab(Frame& out) {
  if (!inner_.grab(out)) return false;
  out.cam_id = cfg_.cam_id;  // must be 2 for Ground EO wire
  out.source_role = static_cast<uint8_t>(SourceRole::GroundEo);
  return true;
}

}  // namespace actprove::capture
