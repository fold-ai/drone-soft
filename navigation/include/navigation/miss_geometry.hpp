#pragma once
// Commanded-miss geometry. V1 G3: 15–30 m lateral miss. Never impact (0 m).
// Owner: Navigation.

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace navigation {

enum class MissSide : uint8_t { kLeft = 0, kRight = 1 };

struct MissConfig {
  float distance_m = 20.f;  // must stay in [15, 30]
  float distance_m_min = 15.f;
  float distance_m_max = 30.f;
  MissSide side = MissSide::kLeft;
  float vertical_offset_m = 0.f;
};

struct NedOffset {
  float north_m = 0.f;
  float east_m = 0.f;
  float down_m = 0.f;
};

inline void validate_miss(const MissConfig& c) {
  if (!(c.distance_m >= c.distance_m_min && c.distance_m <= c.distance_m_max)) {
    throw std::invalid_argument("miss distance must be in [15, 30] m for V1");
  }
  if (!(c.distance_m > 0.f)) {
    throw std::invalid_argument("miss distance must be > 0 (no stick/kill)");
  }
}

// Horizontal miss offset perpendicular to ground-track / LOS yaw (rad, NED).
inline NedOffset miss_offset_ned(const MissConfig& c, float yaw_ned_rad) {
  validate_miss(c);
  const float sign = (c.side == MissSide::kLeft) ? -1.f : 1.f;
  const float yaw_perp = yaw_ned_rad + sign * static_cast<float>(M_PI_2);
  NedOffset o;
  o.north_m = c.distance_m * std::cos(yaw_perp);
  o.east_m = c.distance_m * std::sin(yaw_perp);
  o.down_m = -c.vertical_offset_m;  // +up → −down in NED
  return o;
}

}  // namespace navigation
