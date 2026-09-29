#pragma once
// Heading + commanded-miss guidance. CLOSE only. No stick/kill.
// Owner: Navigation.

#include <utility>

#include "navigation/mavlink_setpoint.hpp"
#include "navigation/miss_geometry.hpp"
#include "navigation/track_input.hpp"

namespace navigation {

struct OwnshipState {
  double t_pps = 0.0;
  float vn_mps = 0.f;
  float ve_mps = 0.f;
  float vd_mps = 0.f;
  float yaw_ned_rad = 0.f;
  float alt_m = 0.f;
};

struct GuidanceConfig {
  MissConfig miss{};
  float approach_speed_mps = 45.f;
  float ownship_speed_mps_max = 55.6f;
  float setpoint_hz = 15.f;
  float max_track_age_s = 1.5f;
};

class HeadingGuidance {
 public:
  explicit HeadingGuidance(GuidanceConfig cfg = {}) : cfg_(std::move(cfg)) {
    validate_miss(cfg_.miss);
  }

  VelYawSetpoint update(const TrackMsg& track, const OwnshipState& own);

  const GuidanceConfig& config() const { return cfg_; }

 private:
  GuidanceConfig cfg_;
};

}  // namespace navigation
