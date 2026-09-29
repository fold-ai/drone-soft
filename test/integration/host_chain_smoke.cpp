// Host-only interface contract test. This is not hardware, FC-log, or flight
// evidence; it verifies that the canonical messages compose across modules.

#include "navigation/heading_guidance.hpp"
#include "navigation/mavlink_setpoint.hpp"
#include "safety_gates/mission_sm.h"
#include "tracking/tracker.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

actprove::DetectionMsg detection(double t_pps, uint64_t seq) {
  actprove::DetectionMsg d{};
  d.t_pps = t_pps;
  d.seq = seq;
  d.cam_id = static_cast<uint8_t>(actprove::CamId::kEo);
  d.src_w = 1920;
  d.src_h = 1200;
  d.n = 1;
  d.boxes[0] = {900.f, 500.f, 50.f, 40.f, 0.9f,
                static_cast<uint32_t>(actprove::ClassId::kGeran2)};
  return d;
}

}  // namespace

int main() {
  tracking::Tracker tracker;
  actprove::TrackMsg track{};
  track = tracker.update(detection(100.00, 1));
  track = tracker.update(detection(100.03, 2));
  track = tracker.update(detection(100.06, 3));
  assert(track.state == actprove::TrackState::kConfirmed);
  assert(track.onboard_owns);
  assert(track.measurement_epoch == track.t_pps);

  ap_mission_sm_t sm;
  ap_mission_sm_init(&sm, nullptr);
  ap_mission_sm_inputs_t in{};
  in.heartbeat_ok = 1;
  in.bit_ok = 1;
  in.corridor_loaded = 1;
  ap_mission_sm_tick(&sm, 100.00, &in);
  assert(ap_mission_sm_handle_command(&sm, AP_UPLINK_NAME_WORK) == 0);
  ap_mission_sm_tick(&sm, 100.01, &in);
  assert(ap_mission_sm_state(&sm) == AP_MISSION_SEARCH);
  in.track_box_valid = 1;
  ap_mission_sm_tick(&sm, 100.02, &in);
  assert(ap_mission_sm_state(&sm) == AP_MISSION_LOCK);
  in.geometry_ok_for_close = 1;
  ap_mission_sm_tick(&sm, 100.03, &in);
  assert(ap_mission_sm_state(&sm) == AP_MISSION_CLOSE);

  track.bearing_ned_rad[0] = 0.2f;
  track.range_est_m = 200.f;
  navigation::OwnshipState own{};
  own.t_pps = 100.07;
  navigation::HeadingGuidance guidance;
  const navigation::VelYawSetpoint sp = guidance.update(track, own);
  assert(sp.valid);
  assert(sp.type_mask == navigation::kTypeMaskVelYaw);
  assert((sp.type_mask & 0x0038u) == 0);  // vx/vy/vz enabled

  const auto wire = navigation::to_wire(sp, 1234);
  assert(wire.type_mask == 0x09C7u);
  assert(std::isfinite(wire.vx));
  assert(std::isfinite(wire.vy));
  assert(std::isfinite(wire.yaw));

  std::puts("host_chain_smoke OK (not hardware evidence)");
  return 0;
}
