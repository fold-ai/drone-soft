#pragma once
// Nose-camera pursuit for a fixed wing.
// Positive azimuth is target-right. Positive elevation is target-below.
// Outputs are bank (positive = right wing down) and pitch (positive = nose up).
// Both flaperons up already pitches the nose up on this airframe, and the plane
// has been flown on the sticks, so the Pixhawk mixer is left as flown.
// This process does not drive servos.

#include <actprove/track_msg.hpp>

#include <cstdint>

namespace actprove::companion {

struct HomingConfig {
  float hfov_deg{10.f};  // JAI GOX-5103 + Kowa 50mm on 2/3" ≈ 10°
  float known_width_m{1.0f};    // flying-wing span; range = span * focal / box width
  float balloon_width_m{1.2f};  // class_id 0
  float max_bank_deg{25.f};
  float max_pitch_up_deg{12.f};
  float max_pitch_down_deg{10.f};
  float trim_pitch_deg{3.f};
  float roll_gain{4.5f};   // bank rad per azimuth rad
  float pitch_gain{4.0f};  // nose rad per elevation rad
  float cruise_throttle{0.08f};  // empty-frame search only; ~100 km/h level
  float chase_throttle{0.90f};   // as soon as a target is seen, accelerate (airframe can reach ~770 km/h)
  float range_near_m{80.f};
  float range_far_m{1000.f};     // camera sees the target out to about 1 km
  float search_bank_deg{12.f};
  float search_period_s{5.f};
};

struct HomingOutput {
  float roll_rad{0.f};
  float pitch_rad{0.f};
  float throttle{0.f};
  float az_rad{0.f};
  float el_rad{0.f};
  float range_est_m{0.f};
  bool valid{false};
};

HomingOutput image_homing(const actprove::TrackMsg& track, uint16_t src_w,
                          uint16_t src_h, const HomingConfig& cfg);

}  // namespace actprove::companion
