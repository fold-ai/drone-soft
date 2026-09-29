#include "navigation/cue_heading.hpp"

#include <cmath>

namespace navigation {

VelYawSetpoint CueHeadingBias::update(const GroundCue& cue,
                                      const OwnshipState& own) const {
  VelYawSetpoint out;
  out.t_pps = cue.t_pps;
  if (!cue_acceptable(cue, cfg_.accept)) {
    out.valid = false;
    return out;
  }

  const float yaw = cue.bearing_az_ned_rad;
  const float v = cfg_.bias_speed_mps;
  out.vx_mps = v * std::cos(yaw);
  out.vy_mps = v * std::sin(yaw);
  out.vz_mps = 0.f;
  out.yaw_rad = yaw;
  out.type_mask = kTypeMaskVelYaw;
  out.coordinate_frame = kMavFrameLocalNed;
  out.valid = true;
  (void)own;
  return out;
}

}  // namespace navigation
