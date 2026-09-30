#include "companion/image_homing.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace actprove::companion {
namespace {

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(v, hi));
}

float deg_to_rad(float deg) { return deg * static_cast<float>(M_PI) / 180.f; }

}  // namespace

HomingOutput image_homing(const actprove::TrackMsg& track, uint16_t src_w,
                          uint16_t src_h, const HomingConfig& cfg) {
  HomingOutput out;
  if (src_w < 16 || src_h < 16) return out;
  if (!std::isfinite(track.u) || !std::isfinite(track.v)) return out;
  if (track.bbox_w < 1.f || track.bbox_h < 1.f) return out;

  const float cx = 0.5f * static_cast<float>(src_w);
  const float cy = 0.5f * static_cast<float>(src_h);
  const float hfov = std::max(5.f, cfg.hfov_deg);
  const float focal_px = cx / std::tan(0.5f * deg_to_rad(hfov));
  if (!(focal_px > 1.f)) return out;

  out.az_rad = std::atan2(track.u - cx, focal_px);
  out.el_rad = std::atan2(track.v - cy, focal_px);
  if (cfg.known_width_m > 0.01f && track.bbox_w > 1.f) {
    out.range_est_m = (cfg.known_width_m * focal_px) / track.bbox_w;
  } else {
    out.range_est_m = std::numeric_limits<float>::quiet_NaN();
  }

  const float max_bank = deg_to_rad(std::max(5.f, cfg.max_bank_deg));
  const float pitch_up = deg_to_rad(std::max(2.f, cfg.max_pitch_up_deg));
  const float pitch_dn = deg_to_rad(std::max(2.f, cfg.max_pitch_down_deg));
  const float trim = deg_to_rad(cfg.trim_pitch_deg);
  out.roll_rad = clampf(cfg.roll_gain * out.az_rad, -max_bank, max_bank);
  out.pitch_rad = clampf(trim - cfg.pitch_gain * out.el_rad, -pitch_dn, pitch_up);

  // A box in the frame means the target is inside the 1 km camera range.
  // Accelerate immediately and steer in the same command. Search (no box)
  // stays at cruise so the turbine is not opened on an empty sky.
  const float cruise = clampf(cfg.cruise_throttle, 0.f, 1.f);
  const float chase = clampf(std::max(cruise, cfg.chase_throttle), 0.f, 1.f);
  const float far_m = std::max(100.f, cfg.range_far_m);
  float effort = 1.f;
  if (std::isfinite(out.range_est_m)) {
    const float far_frac = clampf(out.range_est_m / far_m, 0.f, 1.f);
    effort = std::max(0.75f, far_frac);
  }
  out.throttle = cruise + effort * (chase - cruise);
  out.valid = true;
  return out;
}

}  // namespace actprove::companion
