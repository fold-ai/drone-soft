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

  float speed = std::max(1.f, cfg.close_speed_mps);
  if (std::isfinite(out.range_est_m) && out.range_est_m < 8.f) {
    speed = std::max(6.f, speed * (out.range_est_m / 8.f));
  }
  out.vx = speed;
  out.vy = clampf(cfg.kp_az * out.az_rad * speed, -cfg.max_lateral_mps,
                  cfg.max_lateral_mps);
  out.vz = clampf(cfg.kp_el * out.el_rad * speed, -cfg.max_lateral_mps,
                  cfg.max_lateral_mps);
  out.valid = true;
  return out;
}

}  // namespace actprove::companion
