#pragma once
// Post-CLOSE BDA exit selection for Navigation.
// Safety owns SM; Nav picks the flight profile.
//
// TARGET_DESTROYED → always RTB (clean handoff from CLOSE/ABORT).
//   Do NOT fight LAND_SOFT on BDA-destroyed — soft-land is only for auto
//   TEST_RECOVER after CLOSE miss (land_soft_active without destroyed mark).
// MISS/REATTACK → stop CLOSE, accept re-cue heading.
// Spec: docs/notes/GROUND_EO_CUE_AND_BDA.md + navigation/docs/ground_cue_and_bda.md

#include <cstdint>
#include <actprove/ground_cue_msg.hpp>

namespace navigation {

using BdaMark = actprove::BdaMark;

enum class NavExitMode : uint8_t {
  kIdle = 0,
  kCueHeading = 1,  // bearing bias (SEARCH/LOCK / re-cue)
  kCloseMiss = 2,   // commanded-miss guidance
  kRtb = 3,         // return / recovery
  kSoftLand = 4,    // TEST_RECOVER ABORT submode (auto post-miss only)
};

struct BdaExitInput {
  BdaMark bda = BdaMark::kNone;
  bool land_soft_active = false;  // ap_mission_sm_land_soft_active()
  bool close_active = false;
  bool rtb_active = false;
  bool nose_track_usable = false;
  bool cue_usable = false;
};

struct BdaExitDecision {
  NavExitMode mode = NavExitMode::kIdle;
  bool clear_close_guidance = false;
  bool accept_recue = false;
  const char* reason = "";
};

inline BdaExitDecision select_nav_exit(const BdaExitInput& in) {
  BdaExitDecision d;

  // BDA destroyed always wins → RTB (never compete with LAND_SOFT).
  if (in.bda == BdaMark::kTargetDestroyed) {
    d.mode = NavExitMode::kRtb;
    d.clear_close_guidance = true;
    d.reason = "TARGET_DESTROYED_RTB";
    return d;
  }

  // Auto TEST_RECOVER soft-land after CLOSE miss (SM flag, no destroyed mark).
  if (in.land_soft_active) {
    d.mode = NavExitMode::kSoftLand;
    d.clear_close_guidance = true;
    d.reason = "TEST_RECOVER_SOFT_LAND";
    return d;
  }

  if (in.rtb_active) {
    d.mode = NavExitMode::kRtb;
    d.clear_close_guidance = true;
    d.reason = "SM_RTB";
    return d;
  }

  if (in.bda == BdaMark::kMiss) {
    d.clear_close_guidance = true;
    d.accept_recue = true;
    if (in.cue_usable) {
      d.mode = NavExitMode::kCueHeading;
      d.reason = "MISS_REATTACK_RECUE";
    } else {
      d.mode = NavExitMode::kIdle;
      d.reason = "MISS_WAIT_CUE_OR_WORK";
    }
    return d;
  }

  if (in.close_active && in.nose_track_usable) {
    d.mode = NavExitMode::kCloseMiss;
    d.reason = "CLOSE_NOSE_TRACK";
    return d;
  }

  if (in.cue_usable) {
    d.mode = NavExitMode::kCueHeading;
    d.reason = "CUE_HEADING_BIAS";
    return d;
  }

  d.reason = "IDLE";
  return d;
}

}  // namespace navigation
