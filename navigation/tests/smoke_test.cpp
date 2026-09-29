#include "navigation/adis_sync.hpp"
#include "navigation/heading_guidance.hpp"
#include "navigation/miss_geometry.hpp"
#include "navigation/soft_land_profile.hpp"
#include "navigation/ground_cue.hpp"
#include "navigation/bda_exit.hpp"
#include "navigation/cue_heading.hpp"
#include "navigation/rtb_profile.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
  using namespace navigation;

  MissConfig miss;
  miss.distance_m = 20.f;
  miss.side = MissSide::kLeft;
  const NedOffset o = miss_offset_ned(miss, /*yaw=*/0.f);
  assert(std::abs(std::hypot(o.north_m, o.east_m) - 20.f) < 1e-3f);

  bool threw = false;
  try {
    MissConfig bad;
    bad.distance_m = 0.f;
    validate_miss(bad);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  HeadingGuidance guidance;
  TrackMsg track;
  track.t_pps = 100.0;
  track.measurement_epoch = track.t_pps;
  track.state = TrackState::kConfirmed;
  track.onboard_owns = true;
  track.cam_id = static_cast<uint8_t>(actprove::CamId::kEo);
  track.lock_quality = 0.9f;
  track.bearing_ned_rad[0] = 0.f;
  track.range_est_m = 200.f;
  OwnshipState own;
  own.t_pps = 100.0;
  const VelYawSetpoint sp = guidance.update(track, own);
  assert(sp.valid);
  assert(sp.type_mask == kTypeMaskVelYaw);
  // MAVLink mask must use vx/vy/vz+yaw and ignore pos/accel+yaw-rate.
  assert((sp.type_mask & 0x0007u) == 0x0007u);  // position ignored
  assert((sp.type_mask & 0x0038u) == 0x0000u);  // velocity enabled
  assert((sp.type_mask & 0x01C0u) == 0x01C0u);  // acceleration ignored
  assert((sp.type_mask & 0x0400u) == 0x0000u);  // yaw enabled
  assert((sp.type_mask & 0x0800u) == 0x0800u);  // yaw-rate ignored

  TrackMsg unsafe_track = track;
  unsafe_track.onboard_owns = false;
  assert(!guidance.update(unsafe_track, own).valid);
  unsafe_track = track;
  unsafe_track.bearing_ned_rad[0] = NAN;
  assert(!guidance.update(unsafe_track, own).valid);
  unsafe_track = track;
  OwnshipState stale_own = own;
  stale_own.t_pps = track.measurement_epoch + 2.0;
  assert(!guidance.update(unsafe_track, stale_own).valid);

  assert(adis_mode_acceptable(ImuSyncMode::kFreeRunDrdyHwTs));
  assert(!adis_mode_acceptable(ImuSyncMode::kCamFsyncSync));

  // Soft-land: inactive when flag false
  SoftLandProfile soft;
  SoftLandInput in;
  in.land_soft_active = false;
  in.t_pps = 1.0;
  SoftLandOutput out = soft.update(in);
  assert(out.phase == SoftLandPhase::kInactive);
  assert(!out.setpoint.valid);

  // Active → bleed → descend → disarm
  in.land_soft_active = true;
  in.airspeed_mps = 40.f;
  in.yaw_ned_rad = 0.f;
  in.own.vn_mps = 40.f;
  in.own.ve_mps = 0.f;
  in.alt_agl_m = 50.f;
  out = soft.update(in);
  assert(out.phase == SoftLandPhase::kBleedSpeed);
  assert(out.setpoint.valid);
  assert(std::hypot(out.setpoint.vx_mps, out.setpoint.vy_mps) <= 18.f + 1e-3f);

  in.t_pps = 10.0;  // past bleed_duration
  in.airspeed_mps = 15.f;
  out = soft.update(in);
  assert(out.phase == SoftLandPhase::kDescend);
  assert(out.setpoint.vz_mps >= 1.f && out.setpoint.vz_mps <= 2.f);

  in.t_pps = 20.0;
  in.weight_on_wheels = true;
  in.alt_agl_m = 0.5f;
  out = soft.update(in);
  assert(out.phase == SoftLandPhase::kDisarmPending ||
         out.phase == SoftLandPhase::kDone);
  out = soft.update(in);
  assert(out.request_disarm);
  assert(out.phase == SoftLandPhase::kDone);
  // Done remains latched while LAND_SOFT is active.
  out = soft.update(in);
  assert(out.phase == SoftLandPhase::kDone);
  assert(out.request_disarm);

  // Timeout alone at flight altitude must never request disarm.
  SoftLandConfig guard_cfg;
  guard_cfg.bleed_duration_s = 0.f;
  guard_cfg.disarm_timeout_s = 1.f;
  SoftLandProfile guarded(guard_cfg);
  SoftLandInput guard_in;
  guard_in.land_soft_active = true;
  guard_in.t_pps = 1.0;
  guard_in.airspeed_mps = 18.f;
  guard_in.alt_agl_m = 100.f;
  out = guarded.update(guard_in);
  assert(out.phase == SoftLandPhase::kDescend);
  guard_in.t_pps = 10.0;
  guard_in.airspeed_mps = 0.f;
  out = guarded.update(guard_in);
  assert(out.phase == SoftLandPhase::kDescend);
  assert(!out.request_disarm);

  // Flag drop resets
  in.land_soft_active = false;
  out = soft.update(in);
  assert(out.phase == SoftLandPhase::kInactive);

  std::cout << "navigation_smoke OK (miss + soft-land + cue/BDA/RTB)\n";
  
  // --- Ground cue: bearing-only OK; map-pin reject ---
  GroundCue cue;
  cue.t_pps = 1.0;
  cue.source = CueSource::kGroundEo;
  cue.has_image_box = true;
  cue.cue_quality = 0.8f;
  cue.bearing_az_ned_rad = 0.5f;
  cue.range_est_m = NAN;  // bearing-only
  assert(cue_is_bearing_only(cue));
  assert(cue_acceptable(cue));

  GroundCue pin = cue;
  pin.has_image_box = false;
  assert(!cue_acceptable(pin));

  CueHeadingBias bias;
  OwnshipState own2;
  VelYawSetpoint cue_sp = bias.update(cue, own2);
  assert(cue_sp.valid);

  // --- BDA exits ---
  BdaExitInput bin;
  bin.bda = BdaMark::kTargetDestroyed;
  auto d = select_nav_exit(bin);
  assert(d.mode == NavExitMode::kRtb);
  assert(d.clear_close_guidance);

  // Destroyed must not fight LAND_SOFT — still RTB
  bin.land_soft_active = true;
  d = select_nav_exit(bin);
  assert(d.mode == NavExitMode::kRtb);

  // Soft-land only when flag set without destroyed mark (auto TEST_RECOVER)
  bin = {};
  bin.land_soft_active = true;
  d = select_nav_exit(bin);
  assert(d.mode == NavExitMode::kSoftLand);

  bin = {};
  bin.bda = BdaMark::kMiss;
  bin.cue_usable = true;
  d = select_nav_exit(bin);
  assert(d.mode == NavExitMode::kCueHeading);
  assert(d.accept_recue);

  // --- RTB stub ---
  RtbProfile rtb;
  RtbInput rin;
  rin.rtb_active = true;
  rin.t_pps = 2.0;
  rin.pos_n_m = 100.f;
  rin.pos_e_m = 0.f;
  auto rout = rtb.update(rin);
  assert(rout.setpoint.valid);
  assert(rout.setpoint.vx_mps < 0.f);  // home at 0,0 → south-ish from +N

return 0;
}
