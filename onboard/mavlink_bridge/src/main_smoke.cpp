#include "mavlink_bridge/bridge.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
  actprove::mavlink_bridge::BridgeConfig cfg;
  cfg.smoke = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) cfg.port = argv[++i];
    else if (a == "--baud" && i + 1 < argc) cfg.baud = std::atoi(argv[++i]);
    else if (a == "--smoke") cfg.smoke = true;
  }
  actprove::mavlink_bridge::Bridge bridge(cfg);
  const bool ok = bridge.open();
  std::printf("mavlink_bridge smoke port=%s baud=%d open=%d\n",
              cfg.port.c_str(), cfg.baud, ok ? 1 : 0);
  // Always emit BOOT state + a few heartbeats (null sink OK on host).
  bridge.send_mission_state(0);
  for (int i = 0; i < 3; ++i) {
    bridge.send_heartbeat();
    bridge.spin_once();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::printf("HEARTBEAT x3 + MISSION_STATE=BOOT sent\n");
  std::printf("Pass criteria: see docs/bringup/mavlink_telem2_check.md\n");
  return 0;
}
