#pragma once
// RTB / recovery setpoint stub after TARGET_DESTROYED (or SM RTB).
// Soft-land takes priority when land_soft_active. Owner: Navigation.

#include "navigation/heading_guidance.hpp"
#include "navigation/mavlink_setpoint.hpp"

namespace navigation {

struct RtbConfig {
  float cruise_speed_mps = 25.f;
  float descent_rate_mps = 0.f;  // level RTB default; tune later
  float home_n_m = 0.f;         // NED home relative (stub)
  float home_e_m = 0.f;
};

struct RtbInput {
  bool rtb_active = false;
  double t_pps = 0.0;
  float yaw_ned_rad = 0.f;
  float pos_n_m = 0.f;
  float pos_e_m = 0.f;
  OwnshipState own{};
};

struct RtbOutput {
  VelYawSetpoint setpoint{};
  const char* reason = "";
};

class RtbProfile {
 public:
  explicit RtbProfile(RtbConfig cfg = {}) : cfg_(cfg) {}

  RtbOutput update(const RtbInput& in);

 private:
  RtbConfig cfg_;
};

}  // namespace navigation
