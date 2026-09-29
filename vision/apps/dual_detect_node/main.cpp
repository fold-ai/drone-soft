#include "capture/dual_camera_capture.hpp"
#include "detect/detector.hpp"
#include "iface/detection_ipc.hpp"

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
constexpr bool kHostDefaultSimulate = true;
#else
constexpr bool kHostDefaultSimulate = false;
#endif

volatile std::sig_atomic_t g_stop_requested = 0;

void request_stop(int) {
  g_stop_requested = 1;
}

actprove::iface::DetectionMsg to_message(const actprove::detect::DetectionBatch& batch) {
  actprove::iface::DetectionMsg msg{};
  msg.t_pps = batch.t_pps;
  msg.seq = batch.seq;
  msg.cam_id = batch.cam_id;
  msg.src_w = batch.src_w;
  msg.src_h = batch.src_h;
  msg.n = std::min(batch.n, actprove::iface::MAX_DET);
  for (std::uint32_t i = 0; i < msg.n; ++i) {
    msg.boxes[i].x = batch.boxes[i].x;
    msg.boxes[i].y = batch.boxes[i].y;
    msg.boxes[i].w = batch.boxes[i].w;
    msg.boxes[i].h = batch.boxes[i].h;
    msg.boxes[i].conf = batch.boxes[i].conf;
    msg.boxes[i].class_id = batch.boxes[i].class_id;
  }
  return msg;
}
}

int main(int argc, char** argv) {
  std::string search = "/dev/video0";
  std::string tele = "/dev/video1";
  std::string engine = "models/engines/yolov8n_fp16.engine";
  bool simulate = kHostDefaultSimulate;
  bool require_tele = false;
  // Zero means run until SIGINT/SIGTERM. A finite value remains useful for
  // camera benches and CI.
  int frames = 0;
  int max_failures = 5;
  std::string ipc_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--search" && i + 1 < argc) { search = argv[++i]; simulate = false; }
    else if (arg == "--tele" && i + 1 < argc) tele = argv[++i];
    else if (arg == "--engine" && i + 1 < argc) engine = argv[++i];
    else if (arg == "--frames" && i + 1 < argc) frames = std::max(0, std::atoi(argv[++i]));
    else if (arg == "--max-failures" && i + 1 < argc) max_failures = std::max(1, std::atoi(argv[++i]));
    else if (arg == "--ipc" && i + 1 < argc) ipc_path = argv[++i];
    else if (arg == "--simulate") simulate = true;
    else if (arg == "--require-tele") require_tele = true;
  }


  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);

  actprove::capture::DualCameraConfig capture_config;
  capture_config.search.device = search;
  capture_config.search.simulate = simulate;
  capture_config.tele.device = tele;
  capture_config.tele.simulate = simulate;
  capture_config.allow_single_camera = !require_tele;
  actprove::capture::DualCameraCapture capture(capture_config);
  actprove::detect::Detector detector(engine, simulate);
  if (!capture.open() || !detector.load()) return 1;
  std::unique_ptr<actprove::iface::UnixDetectionPublisher> publisher;
  if (!ipc_path.empty()) {
    publisher = std::make_unique<actprove::iface::UnixDetectionPublisher>(ipc_path);
    if (!publisher->open()) return 6;
  }

  std::printf("vision mode=%s search=%s tele=%s\n",
              capture.tele_available() ? "dual" : "search-only",
              search.c_str(), tele.c_str());
  actprove::capture::Frame frame;
  actprove::detect::DetectionBatch detections;
  int processed = 0;
  int consecutive_failures = 0;
  int reconnects = 0;
  int window_frames = 0;
  double window_infer_ms = 0.0;
  auto window_start = std::chrono::steady_clock::now();
  while (!g_stop_requested && (frames == 0 || processed < frames)) {
    if (!capture.grab_search(frame)) {
      capture.close();
      ++reconnects;
      if (++consecutive_failures >= max_failures) return 2;
      std::this_thread::sleep_for(std::chrono::milliseconds(250 * consecutive_failures));
      if (!capture.open()) continue;
      continue;
    }
    const auto search_infer_start = std::chrono::steady_clock::now();
    if (!detector.infer(frame, detections)) {
      if (++consecutive_failures >= max_failures) return 3;
      continue;
    }
    const auto search_infer_end = std::chrono::steady_clock::now();
    window_infer_ms +=
        std::chrono::duration<double, std::milli>(search_infer_end - search_infer_start).count();
    ++window_frames;
    if (publisher) (void)publisher->publish(to_message(detections));
    std::printf("SEARCH seq=%llu n=%u\n",
                static_cast<unsigned long long>(detections.seq), detections.n);
    if (capture.tele_available()) {
      if (!capture.grab_tele(frame)) {
        capture.close();
        ++reconnects;
        if (++consecutive_failures >= max_failures) return 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * consecutive_failures));
        if (!capture.open()) continue;
        continue;
      }
      const auto tele_infer_start = std::chrono::steady_clock::now();
      if (!detector.infer(frame, detections)) {
        if (++consecutive_failures >= max_failures) return 5;
        continue;
      }
      const auto tele_infer_end = std::chrono::steady_clock::now();
      window_infer_ms +=
          std::chrono::duration<double, std::milli>(tele_infer_end - tele_infer_start).count();
      ++window_frames;
      if (publisher) (void)publisher->publish(to_message(detections));
      std::printf("TELE   seq=%llu n=%u\n",
                  static_cast<unsigned long long>(detections.seq), detections.n);
    }
    consecutive_failures = 0;
    std::fflush(stdout);
    ++processed;
    const auto now = std::chrono::steady_clock::now();
    const double window_s = std::chrono::duration<double>(now - window_start).count();
    if (window_s >= 1.0) {
      std::printf(
          "METRIC streams=%d inference_fps=%.2f inference_mean_ms=%.3f reconnects=%d ipc_sent=%llu ipc_dropped=%llu\n",
          capture.tele_available() ? 2 : 1,
          window_frames / window_s,
          window_frames ? window_infer_ms / window_frames : 0.0,
          reconnects,
          static_cast<unsigned long long>(publisher ? publisher->sent() : 0),
          static_cast<unsigned long long>(publisher ? publisher->dropped() : 0));
      std::fflush(stdout);
      window_start = now;
      window_frames = 0;
      window_infer_ms = 0.0;
    }
  }
  return 0;
}
