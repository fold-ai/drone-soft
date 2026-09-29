#pragma once
// MAVLink SET_POSITION_TARGET_LOCAL_NED (msg 84) filler — vel + yaw only.
// common.xml v1. No custom dialect. No kill modes.
// Owner: Navigation (+ onboard mavlink_bridge).

#include <cstdint>

namespace navigation {

constexpr uint8_t kMavFrameLocalNed = 1;  // MAV_FRAME_LOCAL_NED
// POSITION_TARGET_TYPEMASK bits from MAVLink common.xml.
// Ignore position (0-2), acceleration/force (6-8), and yaw-rate (11).
// Velocity (3-5) and yaw (10) intentionally remain enabled.
constexpr uint16_t kIgnorePosition = 0x0007;
constexpr uint16_t kIgnoreAcceleration = 0x01C0;
constexpr uint16_t kIgnoreYawRate = 0x0800;
constexpr uint16_t kTypeMaskVelYaw =
    kIgnorePosition | kIgnoreAcceleration | kIgnoreYawRate;  // 0x09C7

static_assert((kTypeMaskVelYaw & 0x0038) == 0,
              "velocity components must be enabled");
static_assert((kTypeMaskVelYaw & 0x0400) == 0, "yaw must be enabled");

struct VelYawSetpoint {
  double t_pps = 0.0;
  float vx_mps = 0.f;
  float vy_mps = 0.f;
  float vz_mps = 0.f;
  float yaw_rad = 0.f;
  uint16_t type_mask = kTypeMaskVelYaw;
  uint8_t coordinate_frame = kMavFrameLocalNed;
  bool valid = false;  // false → bridge must NOT send
};

struct SetPositionTargetLocalNedWire {
  uint32_t time_boot_ms = 0;
  uint8_t coordinate_frame = kMavFrameLocalNed;
  uint16_t type_mask = kTypeMaskVelYaw;
  float x = 0, y = 0, z = 0;
  float vx = 0, vy = 0, vz = 0;
  float afx = 0, afy = 0, afz = 0;
  float yaw = 0;
  float yaw_rate = 0;
};

inline SetPositionTargetLocalNedWire to_wire(const VelYawSetpoint& s,
                                             uint32_t time_boot_ms) {
  SetPositionTargetLocalNedWire w;
  w.time_boot_ms = time_boot_ms;
  w.coordinate_frame = s.coordinate_frame;
  w.type_mask = s.type_mask;
  w.vx = s.vx_mps;
  w.vy = s.vy_mps;
  w.vz = s.vz_mps;
  w.yaw = s.yaw_rad;
  return w;
}

}  // namespace navigation
