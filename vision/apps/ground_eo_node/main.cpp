/// Ground EO node stub (GCS laptop V4L2): capture → detect OR manual designate → DetectionMsg.
/// Wire cam_id = CamId::GroundEo (2). source_role optional on Frame meta.
#include "capture/ground_eo_capture.hpp"
#include "detect/detector.hpp"
#include "iface/box_message.hpp"
#include <cstdio>
#include <cstring>

namespace {

#ifdef SOFT_NO_JETPACK
constexpr bool kSimulate = true;
#else
constexpr bool kSimulate = false;
#endif

actprove::iface::DetectionMsg make_manual_designate(
    const actprove::capture::Frame& frame,
    float x, float y, float w, float h,
    uint32_t class_id) {
  actprove::iface::DetectionMsg msg{};
  msg.t_pps = frame.t_pps;
  msg.seq = frame.seq;
  msg.cam_id = static_cast<uint8_t>(actprove::iface::CamId::kGroundEo);  // 2
  msg.src_w = frame.w;
  msg.src_h = frame.h;
  msg.n = 1;
  msg.boxes[0] = {x, y, w, h, 1.0f, class_id};
  return msg;
}

bool class_in_range_gate(uint32_t class_id) {
  using C = actprove::iface::ClassId;
  return class_id == static_cast<uint32_t>(C::kShahed136) ||
         class_id == static_cast<uint32_t>(C::kGeran2) ||
         class_id == static_cast<uint32_t>(C::kGerbera);
}

}  // namespace

int main(int argc, char** argv) {
  const bool manual = (argc > 1 && std::strcmp(argv[1], "--manual") == 0);
  actprove::capture::GroundEoCapture cap(
      {"/dev/video0", 30, 1920, 1080, 2, kSimulate});
  actprove::detect::Detector det("models/engines/yolov8n_fp16.engine", kSimulate);
  if (!cap.open()) {
    std::fprintf(stderr, "ground_eo_node: open failed\n");
    return 1;
  }
  if (!manual && !det.load()) {
    std::fprintf(stderr, "ground_eo_node: detector load failed (use --manual)\n");
    return 1;
  }

  actprove::capture::Frame frame;
  for (int i = 0; i < 5; ++i) {
    if (!cap.grab(frame)) continue;
    actprove::iface::DetectionMsg msg{};
    if (manual) {
      const float bw = frame.w * 0.2f;
      const float bh = frame.h * 0.2f;
      msg = make_manual_designate(
          frame, (frame.w - bw) * 0.5f, (frame.h - bh) * 0.5f, bw, bh,
          static_cast<uint32_t>(actprove::iface::ClassId::kShahed136));
    } else {
      actprove::detect::DetectionBatch batch;
      if (!det.infer(frame, batch)) continue;
      msg.t_pps = batch.t_pps;
      msg.seq = batch.seq;
      msg.cam_id = static_cast<uint8_t>(actprove::iface::CamId::kGroundEo);
      msg.src_w = batch.src_w;
      msg.src_h = batch.src_h;
      msg.n = 0;
      for (uint32_t k = 0; k < batch.n && msg.n < actprove::iface::MAX_DET; ++k) {
        if (!class_in_range_gate(batch.boxes[k].class_id)) continue;
        msg.boxes[msg.n].x = batch.boxes[k].x;
        msg.boxes[msg.n].y = batch.boxes[k].y;
        msg.boxes[msg.n].w = batch.boxes[k].w;
        msg.boxes[msg.n].h = batch.boxes[k].h;
        msg.boxes[msg.n].conf = batch.boxes[k].conf;
        msg.boxes[msg.n].class_id = batch.boxes[k].class_id;
        ++msg.n;
      }
    }
    if (msg.n > 0 && !class_in_range_gate(msg.boxes[0].class_id)) {
      std::fprintf(stderr, "ground_eo_node: class gate reject id=%u\n",
                   msg.boxes[0].class_id);
      continue;
    }
    std::printf(
        "ground_eo seq=%llu n=%u t_pps=%.6f cam_id=%u (GroundEo=2) source_role=%u\n",
        (unsigned long long)msg.seq, msg.n, msg.t_pps, msg.cam_id,
        static_cast<unsigned>(frame.source_role));
  }
  return 0;
}
