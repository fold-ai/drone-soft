#include "navigation/rtb_profile.hpp"

#include <cmath>

namespace navigation {

RtbOutput RtbProfile::update(const RtbInput& in) {
  RtbOutput out;
  if (!in.rtb_active) {
    out.reason = "rtb_inactive";
    return out;
  }

  const float dn = cfg_.home_n_m - in.pos_n_m;
  const float de = cfg_.home_e_m - in.pos_e_m;
  const float dist = std::hypot(dn, de);
  float yaw = in.yaw_ned_rad;
  float vn = 0.f;
  float ve = 0.f;
  if (dist > 1.f) {
    yaw = std::atan2(de, dn);
    vn = cfg_.cruise_speed_mps * (dn / dist);
    ve = cfg_.cruise_speed_mps * (de / dist);
  }

  out.setpoint.t_pps = in.t_pps;
  out.setpoint.vx_mps = vn;
  out.setpoint.vy_mps = ve;
  out.setpoint.vz_mps = cfg_.descent_rate_mps;
  out.setpoint.yaw_rad = yaw;
  out.setpoint.type_mask = kTypeMaskVelYaw;
  out.setpoint.coordinate_frame = kMavFrameLocalNed;
  out.setpoint.valid = true;
  out.reason = "RTB_HOME";
  (void)in.own;
  return out;
}

}  // namespace navigation
