#include "capture/dual_camera_capture.hpp"

namespace actprove::capture {

DualCameraCapture::DualCameraCapture(DualCameraConfig cfg)
    : cfg_(std::move(cfg)), search_(cfg_.search), tele_(cfg_.tele) {}

DualCameraCapture::~DualCameraCapture() { close(); }

bool DualCameraCapture::open() {
  search_open_ = search_.open();
  if (!search_open_) return false;
  tele_open_ = tele_.open();
  if (!tele_open_ && !cfg_.allow_single_camera) {
    close();
    return false;
  }
  return true;
}

void DualCameraCapture::close() {
  tele_.close();
  search_.close();
  tele_open_ = false;
  search_open_ = false;
}

bool DualCameraCapture::grab_search(Frame& out) {
  if (!search_open_ || !search_.grab(out)) return false;
  out.camera_role = CameraRole::Search;
  return true;
}

bool DualCameraCapture::grab_tele(Frame& out) {
  if (!tele_open_ || !tele_.grab(out)) return false;
  out.camera_role = CameraRole::Tele;
  return true;
}

}  // namespace actprove::capture
