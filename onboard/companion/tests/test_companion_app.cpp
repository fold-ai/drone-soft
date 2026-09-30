#include "companion/app.hpp"

#include <cassert>
#include <cmath>
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
  cfg.guided_mode = 15;
  cfg.manual_mode = 5;
  actprove::companion::CompanionApp app(cfg);

  uint16_t ch[12];
  for (auto& c : ch) c = 1500;
  ch[6] = 1900;  // Lock
  app.on_rc(ch, 12);
  app.on_fc_heartbeat(true, 5, 10.0);
  app.on_attitude(0.f, 0.05f, 0.4f);
  app.tick(10.0);
  auto st = app.status();
  assert(st.want_guided);
  assert(st.setpoint_valid);
  assert(st.phase == actprove::companion::CompanionPhase::Search);
  assert(st.throttle > 0.05f);
  assert(st.throttle < 0.15f);
  const auto search = app.attitude();
  assert(search.valid);
  assert(std::fabs(search.yaw_rad - 0.4f) < 0.01f);

  for (int i = 0; i < 3; ++i) {
    app.on_detection(box_at(10.0 + 0.03 * i, static_cast<uint64_t>(i + 1), 1400.f, 520.f));
  }
  app.tick(10.2);
  st = app.status();
  assert(st.want_guided);
  assert(st.setpoint_valid);
  assert(st.phase == actprove::companion::CompanionPhase::Close);
  const auto sp = app.attitude();
  assert(sp.valid);
  assert(sp.roll_rad > 0.05f);
  assert(sp.throttle > 0.50f);

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
