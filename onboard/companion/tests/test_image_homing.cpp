#include "companion/image_homing.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  actprove::TrackMsg track{};
  track.u = 960.f;
  track.v = 540.f;
  track.bbox_w = 80.f;
  track.bbox_h = 40.f;
  actprove::companion::HomingConfig cfg;
  auto center = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(center.valid);
  assert(std::fabs(center.az_rad) < 0.02f);
  assert(std::fabs(center.el_rad) < 0.02f);
  assert(std::fabs(center.roll_rad) < 0.05f);
  assert(center.pitch_rad > 0.02f);
  assert(center.throttle > 0.50f);

  track.bbox_w = 12.f;  // same span, much smaller in the frame → farther
  auto far = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(far.valid);
  assert(far.range_est_m > center.range_est_m);
  assert(far.throttle > center.throttle + 0.04f);
  track.bbox_w = 80.f;

  track.u = 1400.f;  // right of center → right-wing-down
  auto right = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(right.valid);
  assert(right.az_rad > 0.02f);
  assert(right.roll_rad > 0.05f);

  track.u = 960.f;
  track.v = 800.f;  // below center → nose down from trim
  auto down = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(down.el_rad > 0.01f);
  assert(down.pitch_rad < center.pitch_rad);

  std::puts("test_image_homing PASS");
  return 0;
}
