#include "capture/dual_camera_capture.hpp"
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
  std::string search = "/dev/video0";
  std::string tele = "/dev/video1";
  bool simulate = false;
  bool simulate_single = false;
  bool require_tele = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--search" && i + 1 < argc) search = argv[++i];
    else if (arg == "--tele" && i + 1 < argc) tele = argv[++i];
    else if (arg == "--simulate") simulate = true;
    else if (arg == "--simulate-single") simulate_single = true;
    else if (arg == "--require-tele") require_tele = true;
  }

  actprove::capture::DualCameraConfig cfg;
  cfg.search.device = search;
  cfg.search.simulate = simulate || simulate_single;
  cfg.tele.device = tele;
  cfg.tele.simulate = simulate;
  cfg.allow_single_camera = !require_tele;
  actprove::capture::DualCameraCapture capture(cfg);
  if (!capture.open()) {
    std::fprintf(stderr, "failed to open SEARCH camera %s\n", search.c_str());
    return 1;
  }

  std::printf("SEARCH=%s TELE=%s mode=%s\n", search.c_str(), tele.c_str(),
              capture.tele_available() ? "dual" : "single-fallback");
  actprove::capture::Frame frame;
  for (int i = 0; i < 30; ++i) {
    if (!capture.grab_search(frame)) return 2;
    if (capture.tele_available() && !capture.grab_tele(frame)) return 3;
  }
  std::printf("capture PASS\n");
  return 0;
}
