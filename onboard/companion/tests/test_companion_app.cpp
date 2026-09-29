#include "companion/app.hpp"

#include <cassert>
#include <cstdio>

static actprove::DetectionMsg box_at(double t, uint64_t seq, float x, float y) {
  actprove::DetectionMsg d{};
  d.t_pps = t;
  d.seq = seq;
  d.cam_id = 0;
  d.src_w = 1920;
  d.src_h = 1080;
  d.n = 1;
  d.boxes[0] = {x, y, 80.f, 50.f, 0.8f, 4};
  return d;
}

int main() {
  actprove::companion::CompanionConfig cfg;
  cfg.require_armed = true;
  actprove::companion::CompanionApp app(cfg);

  uint16_t ch[12];
  for (auto& c : ch) c = 1500;
  ch[6] = 1900;
  app.on_rc(ch, 12);
  app.on_fc_heartbeat(true, 0, 10.0);
  for (int i = 0; i < 3; ++i) {
    app.on_detection(box_at(10.0 + 0.03 * i, static_cast<uint64_t>(i + 1), 980.f, 520.f));
  }
  app.tick(10.2);
  auto st = app.status();
  assert(st.want_guided);
  assert(st.setpoint_valid);
  assert(st.phase == actprove::companion::CompanionPhase::Close);
  const auto sp = app.setpoint();
  assert(sp.vx > 1.f);
  assert(sp.coordinate_frame == 8);

  ch[7] = 1900;  // takeover
  app.on_rc(ch, 12);
  app.tick(10.3);
  st = app.status();
  assert(st.want_manual);
  assert(!st.want_guided);
  assert(!st.setpoint_valid);
  assert(st.phase == actprove::companion::CompanionPhase::Manual);

  std::puts("test_companion_app PASS");
  return 0;
}
