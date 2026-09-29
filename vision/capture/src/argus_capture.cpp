#include "capture/argus_capture.hpp"
#include "capture/v4l2_capture.hpp"
#include <chrono>
#include <memory>

namespace actprove::capture {

extern double latch_mid_exposure_pps(double sof_pps, float exposure_us);

class ArgusCapture::Impl {
 public:
  std::unique_ptr<V4L2Capture> v4l2;
};

ArgusCapture::ArgusCapture(ArgusConfig cfg)
    : cfg_(std::move(cfg)), impl_(std::make_unique<Impl>()) {}
ArgusCapture::~ArgusCapture() { close(); }

bool ArgusCapture::open() {
  if (cfg_.simulate) {
    open_ = true;
    return true;
  }
  if (!cfg_.v4l2_fallback_device.empty()) {
    impl_->v4l2 = std::make_unique<V4L2Capture>(V4L2Config{
        cfg_.v4l2_fallback_device,
        cfg_.fps,
        cfg_.width,
        cfg_.height,
        cfg_.cam_id,
        false,
        "YUYV",
        4,
    });
    open_ = impl_->v4l2->open();
    return open_;
  }
  // The selected flight cameras are USB3/UVC and use the functional V4L2
  // backend above. A CSI sensor must provide a dedicated libargus adapter;
  // never pretend synthetic frames are live camera frames.
  open_ = false;
  return false;
}

void ArgusCapture::close() {
  if (impl_ && impl_->v4l2) impl_->v4l2->close();
  open_ = false;
}

bool ArgusCapture::grab(Frame& out) {
  if (!open_) return false;
  if (impl_->v4l2) return impl_->v4l2->grab(out);
  using clock = std::chrono::steady_clock;
  const double sof = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
  out.device_ptr = nullptr;  // soft: no NVMM
  out.bytes = 0;
  out.stride_bytes = 0;
  out.pixel_format = PixelFormat::Unknown;
  out.memory_kind = MemoryKind::None;
  out.exposure_us = cfg_.exposure_us;
  out.gain = 1.0f;
  out.w = 1920;
  out.h = 1080;
  out.seq = ++seq_;
  out.cam_id = cfg_.cam_id;
  out.source_role = static_cast<uint8_t>(SourceRole::NoseEo);
  out.t_pps = latch_mid_exposure_pps(sof, out.exposure_us);
  return true;
}

}  // namespace actprove::capture
