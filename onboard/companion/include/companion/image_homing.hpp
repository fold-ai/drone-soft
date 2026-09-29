#pragma once
// Nose-camera pursuit: keep the box on the optical axis and close.
// Body NED: +x forward, +y right, +z down.

#include <actprove/track_msg.hpp>

#include <cstdint>

namespace actprove::companion {

struct HomingConfig {
  float close_speed_mps{18.f};
  float max_lateral_mps{8.f};
  float kp_az{1.8f};
  float kp_el{1.4f};
  float hfov_deg{10.f};  // JAI GOX-5103 + Kowa 50mm on 2/3" ≈ 10°
  float known_width_m{0.35f};
};

struct HomingOutput {
  float vx{0.f};
  float vy{0.f};
  float vz{0.f};
  float az_rad{0.f};
  float el_rad{0.f};
  float range_est_m{0.f};
  bool valid{false};
};

HomingOutput image_homing(const actprove::TrackMsg& track, uint16_t src_w,
                          uint16_t src_h, const HomingConfig& cfg);

}  // namespace actprove::companion
