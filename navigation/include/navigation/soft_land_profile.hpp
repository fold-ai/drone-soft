#pragma once
// Soft-land profile for TEST_RECOVER (ABORT submode).
// Owner: Navigation. SM only gates mode via ap_mission_sm_land_soft_active().
// Does NOT invent a new MISSION_STATE — enum stays BOOT..FTS (0–6).
// Spec: docs/notes/MANUAL_LOCK_AND_TEST_LAND.md §B + SAFETY freeze.

#include "navigation/heading_guidance.hpp"
#include "navigation/mavlink_setpoint.hpp"

#include <cstdint>

namespace navigation {

enum class SoftLandPhase : uint8_t {
  kInactive = 0,   // land_soft_active == false → Nav must not run this profile
  kBleedSpeed,     // cap / reduce airspeed
  kDescend,        // gentle −1…−2 m/s, wings level
  kDisarmPending,  // WoW or guarded low/slow timeout → request disarm
  kDone,           // latched while land_soft_active remains true
};

struct SoftLandConfig {
  float airspeed_cap_mps = 18.f;       // approach-class cap (tune later)
  float descent_rate_mps = 1.5f;       // positive number → NED +vz (down)
  float descent_rate_mps_min = 1.f;
  float descent_rate_mps_max = 2.f;
  float bleed_duration_s = 5.f;        // time in bleed before forcing descend
  float disarm_timeout_s = 45.f;       // minimum time before no-WoW fallback
  float wow_alt_m = 2.f;               // max AGL for no-WoW fallback
  float fallback_disarm_airspeed_mps = 5.f;
  bool hold_heading = true;            // wings-level; hold yaw (into-wind later)
};

struct SoftLandInput {
  bool land_soft_active = false;  // from ap_mission_sm_land_soft_active(&sm)
  double t_pps = 0.0;
  float airspeed_mps = 0.f;
  float yaw_ned_rad = 0.f;
  float alt_agl_m = 1e3f;         // NaN/large if unknown
  bool weight_on_wheels = false;
  OwnshipState own{};
};

struct SoftLandOutput {
  SoftLandPhase phase = SoftLandPhase::kInactive;
  VelYawSetpoint setpoint{};      // valid only when phase is Bleed/Descend
  bool request_disarm = false;    // bridge → PX4 force-disarm / LAND complete
  const char* reason = "";        // log string (LAND_SOFT / DISARM_WOW / …)
};

class SoftLandProfile {
 public:
  explicit SoftLandProfile(SoftLandConfig cfg = {});

  SoftLandOutput update(const SoftLandInput& in);

  SoftLandPhase phase() const { return phase_; }
  const SoftLandConfig& config() const { return cfg_; }
  void reset();

 private:
  SoftLandConfig cfg_;
  SoftLandPhase phase_ = SoftLandPhase::kInactive;
  double t_enter_s_ = 0.0;
  double t_descend_s_ = 0.0;
  float hold_yaw_rad_ = 0.f;
};

}  // namespace navigation
