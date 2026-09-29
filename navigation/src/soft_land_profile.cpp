#include "navigation/soft_land_profile.hpp"

#include <algorithm>
#include <cmath>

namespace navigation {
namespace {

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(v, hi));
}

}  // namespace

SoftLandProfile::SoftLandProfile(SoftLandConfig cfg) : cfg_(cfg) {
  cfg_.descent_rate_mps =
      clampf(cfg_.descent_rate_mps, cfg_.descent_rate_mps_min,
             cfg_.descent_rate_mps_max);
}

void SoftLandProfile::reset() {
  phase_ = SoftLandPhase::kInactive;
  t_enter_s_ = 0.0;
  t_descend_s_ = 0.0;
  hold_yaw_rad_ = 0.f;
}

SoftLandOutput SoftLandProfile::update(const SoftLandInput& in) {
  SoftLandOutput out;

  if (!in.land_soft_active) {
    reset();
    out.phase = SoftLandPhase::kInactive;
    out.reason = "inactive";
    return out;
  }

  // Rising edge into LAND_SOFT (ABORT submode).
  if (phase_ == SoftLandPhase::kInactive) {
    phase_ = SoftLandPhase::kBleedSpeed;
    t_enter_s_ = in.t_pps;
    hold_yaw_rad_ = in.yaw_ned_rad;
  }

  const double dt_enter = in.t_pps - t_enter_s_;

  switch (phase_) {
    case SoftLandPhase::kBleedSpeed: {
      out.reason = "LAND_SOFT_BLEED";
      // Cap horizontal speed; keep wings-level heading.
      const float cap = std::max(5.f, cfg_.airspeed_cap_mps);
      float vn = in.own.vn_mps;
      float ve = in.own.ve_mps;
      const float vxy = std::hypot(vn, ve);
      if (vxy > cap && vxy > 1e-3f) {
        const float s = cap / vxy;
        vn *= s;
        ve *= s;
      } else if (vxy < 1e-3f && cfg_.hold_heading) {
        // No groundspeed estimate — command forward at cap along hold yaw.
        vn = cap * std::cos(hold_yaw_rad_);
        ve = cap * std::sin(hold_yaw_rad_);
      }
      out.setpoint.t_pps = in.t_pps;
      out.setpoint.vx_mps = vn;
      out.setpoint.vy_mps = ve;
      out.setpoint.vz_mps = 0.f;
      out.setpoint.yaw_rad = cfg_.hold_heading ? hold_yaw_rad_ : in.yaw_ned_rad;
      out.setpoint.type_mask = kTypeMaskVelYaw;
      out.setpoint.coordinate_frame = kMavFrameLocalNed;
      out.setpoint.valid = true;

      const bool slow_enough = in.airspeed_mps <= cfg_.airspeed_cap_mps * 1.05f;
      if (!(slow_enough || dt_enter >= cfg_.bleed_duration_s)) {
        break;
      }
      phase_ = SoftLandPhase::kDescend;
      t_descend_s_ = in.t_pps;
      [[fallthrough]];
    }
    case SoftLandPhase::kDescend: {
      out.reason = "LAND_SOFT_DESCEND";
      const float cap = std::max(5.f, cfg_.airspeed_cap_mps);
      float vn = cap * std::cos(hold_yaw_rad_);
      float ve = cap * std::sin(hold_yaw_rad_);
      // Scale horizontal with measured airspeed if available.
      if (in.airspeed_mps > 1.f && in.airspeed_mps < cap) {
        const float s = in.airspeed_mps / cap;
        vn *= s;
        ve *= s;
      }
      out.setpoint.t_pps = in.t_pps;
      out.setpoint.vx_mps = vn;
      out.setpoint.vy_mps = ve;
      out.setpoint.vz_mps = cfg_.descent_rate_mps;  // NED down
      out.setpoint.yaw_rad = hold_yaw_rad_;
      out.setpoint.type_mask = kTypeMaskVelYaw;
      out.setpoint.coordinate_frame = kMavFrameLocalNed;
      out.setpoint.valid = true;

      const double dt_desc = in.t_pps - t_descend_s_;
      const bool fallback_ground_likely =
          dt_desc >= cfg_.disarm_timeout_s &&
          std::isfinite(in.alt_agl_m) && in.alt_agl_m <= cfg_.wow_alt_m &&
          std::isfinite(in.airspeed_mps) &&
          in.airspeed_mps <= cfg_.fallback_disarm_airspeed_mps;
      if (in.weight_on_wheels || fallback_ground_likely) {
        phase_ = SoftLandPhase::kDisarmPending;
      }
      break;
    }
    case SoftLandPhase::kDisarmPending: {
      out.reason = in.weight_on_wheels ? "LAND_SOFT_DISARM_WOW"
                                       : "LAND_SOFT_DISARM_TIMEOUT";
      out.request_disarm = true;
      // Hold zero / minimal sink while disarm requested.
      out.setpoint.t_pps = in.t_pps;
      out.setpoint.vx_mps = 0.f;
      out.setpoint.vy_mps = 0.f;
      out.setpoint.vz_mps = 0.f;
      out.setpoint.yaw_rad = hold_yaw_rad_;
      out.setpoint.type_mask = kTypeMaskVelYaw;
      out.setpoint.coordinate_frame = kMavFrameLocalNed;
      out.setpoint.valid = true;
      phase_ = SoftLandPhase::kDone;
      break;
    }
    case SoftLandPhase::kDone: {
      out.reason = "LAND_SOFT_DONE";
      out.request_disarm = true;
      out.setpoint.valid = false;
      break;
    }
    case SoftLandPhase::kInactive:
    default:
      out.reason = "inactive";
      break;
  }

  out.phase = phase_;
  return out;
}

}  // namespace navigation
