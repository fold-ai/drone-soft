// Software closed loop: pilot flies, Lock searches, a wing enters the
// 10° frame, companion banks and pitches onto it, Takeover releases.
// This does not fly the aircraft and does not open a camera.

#include "companion/app.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

static actprove::DetectionMsg box_at(double t, uint64_t seq, float x, float y,
                                     float w, float h) {
  actprove::DetectionMsg d{};
  d.t_pps = t;
  d.seq = seq;
  d.cam_id = 0;
  d.src_w = 2448;
  d.src_h = 2048;
  d.n = 1;
  d.boxes[0] = {x, y, w, h, 0.9f, 1};
  return d;
}

static const char* phase_name(actprove::companion::CompanionPhase p) {
  if (p == actprove::companion::CompanionPhase::Search) return "SEARCH";
  if (p == actprove::companion::CompanionPhase::Close) return "CLOSE";
  return "MANUAL";
}

int main() {
  actprove::companion::CompanionConfig cfg;
  cfg.require_armed = true;
  assert(cfg.guided_mode == 15);
  assert(cfg.manual_mode == 5);
  actprove::companion::CompanionApp app(cfg);

  uint16_t ch[12];
  for (auto& c : ch) c = 1500;
  int fails = 0;
  auto check = [&](bool ok, const char* what) {
    if (!ok) {
      std::fprintf(stderr, "FAIL %s\n", what);
      ++fails;
    }
  };

  app.on_fc_heartbeat(true, 5, 0.0);
  app.on_attitude(0.f, 0.05f, 1.2f);
  app.on_rc(ch, 12);
  app.tick(0.0);
  auto st = app.status();
  std::printf("t=0.0  sticks   phase=%s guided=%d\n", phase_name(st.phase),
              st.want_guided ? 1 : 0);
  check(st.phase == actprove::companion::CompanionPhase::Manual, "sticks stay manual");
  check(!st.setpoint_valid, "no command before lock");

  ch[6] = 1900;  // CH7 Lock
  app.on_rc(ch, 12);
  app.on_fc_heartbeat(false, 5, 1.0);
  app.tick(1.0);
  st = app.status();
  std::printf("t=1.0  lock, disarmed  phase=%s guided=%d\n", phase_name(st.phase),
              st.want_guided ? 1 : 0);
  check(!st.want_guided, "disarmed does not guide");

  app.on_fc_heartbeat(true, 5, 2.0);
  app.tick(2.0);
  st = app.status();
  std::printf("t=2.0  lock, no target  phase=%s roll=%.1f thr=%.2f\n",
              phase_name(st.phase), st.roll_rad * 57.3f, st.throttle);
  check(st.phase == actprove::companion::CompanionPhase::Search, "search with empty frame");
  check(st.throttle > 0.05f && st.throttle < 0.15f, "search throttle is cruise");

  app.on_fc_heartbeat(true, 5, 3.25);
  app.tick(3.25);  // 1.25 s into the 5 s sweep → about +12°
  st = app.status();
  std::printf("t=3.25 search sweep     phase=%s roll=%.1f\n", phase_name(st.phase),
              st.roll_rad * 57.3f);
  check(st.roll_rad > 0.15f, "search banks");

  // White flying-wing sized box, right and above the crosshair.
  uint64_t seq = 1;
  for (int i = 0; i < 4; ++i) {
    const double t = 4.0 + 0.05 * i;
    app.on_fc_heartbeat(true, 5, t);
    app.on_detection(box_at(t, seq++, 1680.f, 780.f, 90.f, 18.f));
    app.tick(t);
  }
  st = app.status();
  const auto chase = app.attitude();
  std::printf("t=4.2  wing in frame   phase=%s roll=%.1f pitch=%.1f thr=%.2f\n",
              phase_name(st.phase), st.roll_rad * 57.3f, st.pitch_rad * 57.3f,
              st.throttle);
  check(st.phase == actprove::companion::CompanionPhase::Close, "lock becomes close");
  check(chase.valid && chase.roll_rad > 0.05f, "banks toward a target on the right");
  check(chase.pitch_rad > 0.08f, "nose up toward a target above center");
  check(std::fabs(chase.yaw_rad - 1.2f) < 0.01f, "yaw stays the aircraft yaw");

  // Walk onto the crosshair in steps the tracker will follow.
  for (int i = 0; i < 6; ++i) {
    const double t = 5.0 + 0.05 * i;
    const float x = 1680.f - (1680.f - 1179.f) * (static_cast<float>(i) / 5.f);
    const float y = 780.f - (780.f - 1015.f) * (static_cast<float>(i) / 5.f);
    app.on_fc_heartbeat(true, 5, t);
    app.on_detection(box_at(t, seq++, x, y, 90.f, 18.f));
    app.tick(t);
  }
  st = app.status();
  std::printf("t=5.2  wing on axis    phase=%s roll=%.1f pitch=%.1f\n",
              phase_name(st.phase), st.roll_rad * 57.3f, st.pitch_rad * 57.3f);
  check(st.phase == actprove::companion::CompanionPhase::Close, "holds close on axis");
  check(std::fabs(st.roll_rad) < std::fabs(chase.roll_rad), "roll eases as the wing centers");

  ch[7] = 1900;  // CH8 Takeover
  app.on_fc_heartbeat(true, 5, 6.0);
  app.on_rc(ch, 12);
  app.tick(6.0);
  st = app.status();
  std::printf("t=6.0  takeover        phase=%s guided=%d command=%d\n",
              phase_name(st.phase), st.want_guided ? 1 : 0, st.setpoint_valid ? 1 : 0);
  check(st.phase == actprove::companion::CompanionPhase::Manual, "takeover returns sticks");
  check(!st.want_guided && !st.setpoint_valid, "commands stop");

  if (fails) {
    std::fprintf(stderr, "intercept scenario FAIL %d\n", fails);
    return 1;
  }
  std::puts("intercept scenario PASS");
  return 0;
}
