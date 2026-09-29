#include "navigation/heading_guidance.hpp"

#include <algorithm>
#include <cmath>

namespace navigation {
namespace {

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(v, hi));
}

float wrap_pi(float a) {
  while (a > static_cast<float>(M_PI)) a -= 2.f * static_cast<float>(M_PI);
  while (a < -static_cast<float>(M_PI)) a += 2.f * static_cast<float>(M_PI);
  return a;
}

}  // namespace

VelYawSetpoint HeadingGuidance::update(const TrackMsg& track,
                                       const OwnshipState& own) {
  VelYawSetpoint out;
  out.t_pps = track.t_pps;
  if (!track_usable_for_close(track)) {
    out.valid = false;
    return out;
  }

  const double epoch = track.measurement_epoch;
  const double age_s = own.t_pps - epoch;
  if (!std::isfinite(epoch) || epoch <= 0.0 ||
      !std::isfinite(track.t_pps) ||
      std::abs(track.t_pps - epoch) > 1e-6 ||
      !std::isfinite(own.t_pps) || age_s < 0.0 ||
      age_s > cfg_.max_track_age_s ||
      !std::isfinite(track.bearing_ned_rad[0])) {
    out.valid = false;
    return out;
  }

  const float los_yaw = track.bearing_ned_rad[0];
  const NedOffset miss = miss_offset_ned(cfg_.miss, los_yaw);
  float speed =
      clampf(cfg_.approach_speed_mps, 5.f, cfg_.ownship_speed_mps_max);

  float vn = speed * std::cos(los_yaw);
  float ve = speed * std::sin(los_yaw);

  if (std::isfinite(track.range_est_m) && track.range_est_m > 1.f) {
    const float miss_norm = std::hypot(miss.north_m, miss.east_m);
    if (miss_norm > 1e-3f) {
      const float lat_n = miss.north_m / miss_norm;
      const float lat_e = miss.east_m / miss_norm;
      const float lateral_gain = clampf(
          (track.range_est_m - cfg_.miss.distance_m) / track.range_est_m, 0.05f,
          0.35f);
      vn += lateral_gain * speed * lat_n;
      ve += lateral_gain * speed * lat_e;
    }
  } else {
    const float yaw_bias =
        (cfg_.miss.side == MissSide::kLeft ? -1.f : 1.f) * 0.05f;
    const float cmd_yaw = wrap_pi(los_yaw + yaw_bias);
    vn = speed * std::cos(cmd_yaw);
    ve = speed * std::sin(cmd_yaw);
  }

  const float vnorm = std::hypot(vn, ve);
  if (vnorm > cfg_.ownship_speed_mps_max && vnorm > 1e-3f) {
    const float s = cfg_.ownship_speed_mps_max / vnorm;
    vn *= s;
    ve *= s;
  }

  out.vx_mps = vn;
  out.vy_mps = ve;
  out.vz_mps = 0.f;
  out.yaw_rad = wrap_pi(std::atan2(ve, vn));
  out.type_mask = kTypeMaskVelYaw;
  out.coordinate_frame = kMavFrameLocalNed;
  out.valid = true;
  return out;
}

}  // namespace navigation
