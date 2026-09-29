#pragma once
// Tracking → Navigation / Safety wire format (ICD: docs/icd/track_msg.md)
// measurement_epoch = t_pps of the frames that formed the update.
// Coast: keep epoch of last real measurement. Soft now() forbidden.
// No PWM from Tracking.

#include <cstdint>
#include <limits>

namespace actprove {

enum class TrackState : uint8_t {
  kNone = 0,
  kTentative = 1,
  kConfirmed = 2,
  kCoast = 3,
  kLost = 4,
};

enum class LockState : uint8_t {
  kNoLock = 0,
  kLock = 1,
  kCoast = 2,
};

// Ground EO → nose EO ownership (docs/notes/GROUND_EO_CUE_AND_BDA.md)
enum class HandoffPhase : uint8_t {
  kIdle = 0,
  kGroundSeeded = 1,  // Operator Lock on Ground EO — cue / soft seed
  kAwaitNose = 2,     // Waiting for nose EO same-class acquire
  kOnboard = 3,       // Nose EO owns track — CLOSE eligible
  kPostMiss = 4,      // BDA MISS — re-seed allowed without BOOT
};

struct TrackMsg {
  uint32_t track_id{0};
  double measurement_epoch{0.0};
  double t_pps{0.0};
  double t_publish_pps{0.0};
  TrackState state{TrackState::kNone};
  LockState lock_state{LockState::kNoLock};
  uint8_t cam_id{0};
  float u{0.f};
  float v{0.f};
  float bbox_w{0.f};
  float bbox_h{0.f};
  float range_est_m{std::numeric_limits<float>::quiet_NaN()};
  float bearing_ned_rad[2]{0.f, 0.f};
  float lock_quality{0.f};
  uint32_t class_id{0};
  uint32_t age_updates{0};
  float time_since_meas_s{0.f};

  HandoffPhase handoff_phase{HandoffPhase::kIdle};
  uint8_t cue_source{0};       // CamId that seeded / last owned
  bool onboard_owns{false};    // true ⇒ CLOSE may use this track
  bool operator_lock_req{false};

  void set_measurement_epoch(double epoch_pps) {
    measurement_epoch = epoch_pps;
    t_pps = epoch_pps;
  }
};

}  // namespace actprove
