#include "detect/detector.hpp"
#include <cstdio>

namespace {
#ifdef SOFT_NO_JETPACK
constexpr bool kSimulate = true;
#else
constexpr bool kSimulate = false;
#endif
}

int main() {
  actprove::detect::Detector det("models/engines/yolov8n_fp16.engine", kSimulate);
  if (!det.load()) return 1;
  actprove::capture::Frame f{};
  f.w = 1920; f.h = 1080; f.t_pps = 1.0; f.seq = 1;
  actprove::detect::DetectionBatch b;
  det.infer(f, b);
  std::printf("offline n=%u\n", b.n);
  return 0;
}
