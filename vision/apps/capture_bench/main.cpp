#include "capture/argus_capture.hpp"
#include <chrono>
#include <cstdio>

namespace {
#ifdef SOFT_NO_JETPACK
constexpr bool kSimulate = true;
#else
constexpr bool kSimulate = false;
#endif
}

int main() {
  actprove::capture::ArgusCapture cap({{}, 60, 300.f, 0, kSimulate});
  if (!cap.open()) return 1;
  actprove::capture::Frame f;
  const auto t0 = std::chrono::steady_clock::now();
  int n = 0;
  while (n < 120) {
    if (cap.grab(f)) ++n;
  }
  const auto ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - t0).count();
  std::printf("frames=%d elapsed_ms=%.1f hz=%.1f last_t_pps=%.6f\n",
              n, ms, n / (ms / 1000.0), f.t_pps);
  return 0;
}
