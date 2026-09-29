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
  assert(center.vx > 1.f);
  assert(std::fabs(center.vy) < 0.5f);

  track.u = 1400.f;  // right of center → +vy (body right)
  auto right = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(right.valid);
  assert(right.az_rad > 0.1f);
  assert(right.vy > 0.5f);

  track.u = 960.f;
  track.v = 800.f;  // below center → +vz (down)
  auto down = actprove::companion::image_homing(track, 1920, 1080, cfg);
  assert(down.el_rad > 0.05f);
  assert(down.vz > 0.2f);

  std::puts("test_image_homing PASS");
  return 0;
}
