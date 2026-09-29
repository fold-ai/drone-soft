#include "iface/detection_ipc.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <unistd.h>

int main() {
  using namespace actprove::iface;
  DetectionMsg original{};
  original.t_pps = 42.25;
  original.seq = 77;
  original.cam_id = 0;
  original.src_w = 1920;
  original.src_h = 1080;
  original.n = 1;
  original.boxes[0] = {10.f, 20.f, 30.f, 40.f, 0.75f, 11};

  const auto wire = encode_detection_v1(original);
  DetectionMsg decoded{};
  assert(decode_detection_v1(wire.data(), wire.size(), decoded));
  assert(decoded.seq == original.seq);
  assert(decoded.n == 1);
  assert(std::fabs(decoded.boxes[0].conf - 0.75f) < 1e-6f);

  auto corrupted = wire;
  corrupted[0] = 'X';
  assert(!decode_detection_v1(corrupted.data(), corrupted.size(), decoded));
  assert(!decode_detection_v1(wire.data(), wire.size() - 1, decoded));

  const std::string path = "/tmp/actprove-detection-ipc-" + std::to_string(::getpid()) + ".sock";
  UnixDetectionSubscriber subscriber(path);
  UnixDetectionPublisher publisher(path);
  assert(subscriber.open());
  assert(publisher.open());
  assert(publisher.publish(original));
  assert(subscriber.receive(decoded, 1000));
  assert(decoded.seq == original.seq);
  assert(decoded.src_w == 1920);
  assert(publisher.sent() == 1);
  assert(publisher.dropped() == 0);
  return 0;
}
