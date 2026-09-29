#include "logging/blackbox.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::string root = "/tmp/actprove_blackbox";
  int seconds = 2;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--out" && i + 1 < argc) root = argv[++i];
    if (a == "--seconds" && i + 1 < argc) seconds = std::atoi(argv[++i]);
  }
  actprove::logging::BlackboxWriter w({root, "", 1 << 20});
  if (!w.open_run()) {
    std::cerr << "open_run failed\n";
    return 1;
  }
  w.write_jsonl("mission_state", "{\"state\":\"BOOT\",\"t_pps\":0.0}");
  actprove::logging::CameraTsRecord cam{};
  cam.measurement_epoch_ns = 1'000'000'000;
  cam.publish_time_ns = 1'000'025'000'000;  // +25 ms demo
  // fix: publish should be epoch + 25ms = 1e9 + 25e6
  cam.publish_time_ns = 1'000'000'000 + 25'000'000;
  cam.seq = 1;
  cam.cam_id = 0;
  cam.exposure_us = 300.0;
  w.write_camera_ts(cam);

  actprove::logging::TrackRecord tr{};
  tr.track_id = 1;
  tr.measurement_epoch_ns = cam.measurement_epoch_ns;
  tr.publish_time_ns = cam.measurement_epoch_ns + 60'000'000;  // +60 ms E2E demo
  tr.lock_quality = 0.0f;
  tr.state = 0;
  tr.cam_id = 0;
  w.write_track(tr);

  std::cout << "run_dir=" << w.run_dir() << "\n";
  (void)seconds;
  return 0;
}
