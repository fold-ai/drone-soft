#include "capture/argus_capture.hpp"
#include "detect/detector.hpp"
#include "iface/detection_ipc.hpp"
#include "iface/box_message.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

namespace {
#ifdef SOFT_NO_JETPACK
constexpr bool kSimulate = true;
#else
constexpr bool kSimulate = false;
#endif

volatile std::sig_atomic_t g_stop_requested = 0;

void request_stop(int) {
  g_stop_requested = 1;
}
}

int main(int argc, char** argv) {
  bool simulate = kSimulate;
  bool force_argus = false;
  // Zero means run until SIGINT/SIGTERM. A finite value remains useful for
  // bench and CI runs.
  int frames = 0;
  int max_failures = 5;
  std::string ipc_path;
  std::string device = "/dev/video0";
  std::string engine = "models/engines/yolov8n_fp16.engine";
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--device" && i + 1 < argc) {
      device = argv[++i];
      simulate = false;
    } else if (arg == "--engine" && i + 1 < argc) {
      engine = argv[++i];
    } else if (arg == "--simulate") {
      simulate = true;
    } else if (arg == "--argus") {
      force_argus = true;
      simulate = false;
    } else if (arg == "--frames" && i + 1 < argc) {
      frames = std::max(0, std::atoi(argv[++i]));
    } else if (arg == "--max-failures" && i + 1 < argc) {
      max_failures = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--ipc" && i + 1 < argc) {
      ipc_path = argv[++i];
    }
  }

  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);
  actprove::capture::ArgusConfig capture_cfg;
  capture_cfg.fps = 60;
  capture_cfg.exposure_us = 300.f;
  capture_cfg.cam_id = 0;
  capture_cfg.simulate = simulate;
  if (!simulate && !force_argus) capture_cfg.v4l2_fallback_device = device;
  actprove::capture::ArgusCapture cap(capture_cfg);
  actprove::detect::Detector det(engine, simulate);
  if (!cap.open() || !det.load()) return 1;
  std::unique_ptr<actprove::iface::UnixDetectionPublisher> publisher;
  if (!ipc_path.empty()) {
    publisher = std::make_unique<actprove::iface::UnixDetectionPublisher>(ipc_path);
    if (!publisher->open()) return 4;
  }
  actprove::capture::Frame frame;
  actprove::detect::DetectionBatch batch;
  int processed = 0;
  int consecutive_failures = 0;
  int window_frames = 0;
  double window_infer_ms = 0.0;
  auto window_start = std::chrono::steady_clock::now();
  while (!g_stop_requested && (frames == 0 || processed < frames)) {
    if (!cap.grab(frame)) {
      if (++consecutive_failures >= max_failures) return 2;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    const auto infer_start = std::chrono::steady_clock::now();
    if (!det.infer(frame, batch)) {
      if (++consecutive_failures >= max_failures) return 3;
      continue;
    }
    const auto infer_end = std::chrono::steady_clock::now();
    consecutive_failures = 0;
    window_infer_ms += std::chrono::duration<double, std::milli>(infer_end - infer_start).count();
    ++window_frames;
    actprove::iface::DetectionMsg msg{};
    msg.t_pps = batch.t_pps;
    msg.seq = batch.seq;
    msg.cam_id = batch.cam_id;
    msg.src_w = batch.src_w;
    msg.src_h = batch.src_h;
    msg.n = std::min(batch.n, actprove::kMaxDet);
    for (uint32_t k = 0; k < msg.n; ++k) {
      msg.boxes[k].x = batch.boxes[k].x;
      msg.boxes[k].y = batch.boxes[k].y;
      msg.boxes[k].w = batch.boxes[k].w;
      msg.boxes[k].h = batch.boxes[k].h;
      msg.boxes[k].conf = batch.boxes[k].conf;
      msg.boxes[k].class_id = batch.boxes[k].class_id;
    }
    if (publisher) (void)publisher->publish(msg);
    std::printf("seq=%llu n=%u t_pps=%.6f\n",
                (unsigned long long)msg.seq, msg.n, msg.t_pps);
    std::fflush(stdout);
    ++processed;
    const double window_s = std::chrono::duration<double>(infer_end - window_start).count();
    if (window_s >= 1.0) {
      std::printf("METRIC stream=EO fps=%.2f inference_mean_ms=%.3f failures=%d ipc_sent=%llu ipc_dropped=%llu\n",
                  window_frames / window_s,
                  window_frames ? window_infer_ms / window_frames : 0.0,
                  consecutive_failures,
                  static_cast<unsigned long long>(publisher ? publisher->sent() : 0),
                  static_cast<unsigned long long>(publisher ? publisher->dropped() : 0));
      std::fflush(stdout);
      window_start = infer_end;
      window_frames = 0;
      window_infer_ms = 0.0;
    }
  }
  return 0;
}
