#include "iface/detection_ipc.hpp"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <string>

namespace {
volatile std::sig_atomic_t g_stop_requested = 0;
void request_stop(int) { g_stop_requested = 1; }
}

int main(int argc, char** argv) {
  std::string socket_path = "/run/actprove/detections.sock";
  std::string output_path = "/var/lib/actprove/detections.jsonl";
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--socket" && i + 1 < argc) socket_path = argv[++i];
    else if (arg == "--out" && i + 1 < argc) output_path = argv[++i];
  }

  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);
  actprove::iface::UnixDetectionSubscriber subscriber(socket_path);
  if (!subscriber.open()) {
    std::fprintf(stderr, "detection_logger: cannot bind %s\n", socket_path.c_str());
    return 1;
  }
  std::ofstream output(output_path, std::ios::app);
  if (!output) {
    std::fprintf(stderr, "detection_logger: cannot open %s\n", output_path.c_str());
    return 2;
  }

  std::uint64_t received = 0;
  while (!g_stop_requested) {
    actprove::iface::DetectionMsg msg{};
    if (!subscriber.receive(msg, 500)) continue;
    output << "{\"t_pps\":" << msg.t_pps << ",\"seq\":" << msg.seq
           << ",\"cam_id\":" << static_cast<unsigned>(msg.cam_id)
           << ",\"src_w\":" << msg.src_w << ",\"src_h\":" << msg.src_h
           << ",\"boxes\":[";
    const std::uint32_t count = std::min(msg.n, actprove::iface::MAX_DET);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (i) output << ',';
      const auto& box = msg.boxes[i];
      output << "{\"x\":" << box.x << ",\"y\":" << box.y
             << ",\"w\":" << box.w << ",\"h\":" << box.h
             << ",\"conf\":" << box.conf << ",\"class_id\":"
             << box.class_id << '}';
    }
    output << "]}\n";
    if (++received % 30 == 0) output.flush();
  }
  output.flush();
  return 0;
}
