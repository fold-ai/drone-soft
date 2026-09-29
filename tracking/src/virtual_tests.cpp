// Virtual tests: ground-EO handoff + class-gated tracks (shahed/geran/gerbera).
#include "tracking/tracker.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_fail = 0;

static void expect(bool ok, const char* name) {
  if (ok) {
    std::printf("PASS  %s\n", name);
  } else {
    std::printf("FAIL  %s\n", name);
    ++g_fail;
  }
}

static actprove::GroundCueMsg makeCue(uint32_t class_id, double t) {
  actprove::GroundCueMsg cue{};
  cue.t_pps = t;
  cue.u = 320.f;
  cue.v = 240.f;
  cue.bbox_w = 40.f;
  cue.bbox_h = 30.f;
  cue.src_w = 640;
  cue.src_h = 480;
  cue.class_id = class_id;
  cue.operator_lock_request = true;
  cue.bearing_ned_rad[0] = 0.1f;
  cue.bearing_ned_rad[1] = -0.05f;
  return cue;
}

static actprove::DetectionMsg makeNose(uint32_t class_id, double t,
                                       uint64_t seq, float conf = 0.85f) {
  actprove::DetectionMsg det{};
  det.t_pps = t;
  det.seq = seq;
  det.cam_id = static_cast<uint8_t>(actprove::CamId::kEo);
  det.src_w = 1920;
  det.src_h = 1200;
  det.n = 1;
  det.boxes[0] = {900.f, 500.f, 50.f, 40.f, conf, class_id};
  return det;
}


static void test_lock_acceptance_scores() {
  tracking::Tracker tr;
  // Low conf - below Lock min_conf 0.45 but above associate floor: no new lock
  actprove::DetectionMsg det{};
  det.t_pps = 1.0;
  det.seq = 1;
  det.cam_id = 0;
  det.src_w = 1920;
  det.src_h = 1200;
  det.n = 1;
  det.boxes[0] = {100.f, 100.f, 40.f, 30.f, 0.30f, 0};
  auto t = tr.update(det);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle && !t.onboard_owns,
         "low conf 0.30: no auto Lock");

  // Tiny box
  tracking::Tracker tr2;
  det.boxes[0] = {100.f, 100.f, 2.f, 2.f, 0.90f, 0};
  t = tr2.update(det);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "tiny box: no auto Lock");

  // Full-frame junk
  tracking::Tracker tr3;
  det.boxes[0] = {0.f, 0.f, 1900.f, 1180.f, 0.99f, 1};
  t = tr3.update(det);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "full-frame frac: no auto Lock");

  // Good box
  tracking::Tracker tr4;
  det.boxes[0] = {100.f, 100.f, 40.f, 30.f, 0.50f, 2};
  for (int i = 0; i < 3; ++i) {
    det.t_pps = 1.0 + 0.033 * i;
    det.seq = static_cast<uint64_t>(i + 1);
    t = tr4.update(det);
  }
  expect(t.state == actprove::TrackState::kConfirmed && t.onboard_owns,
         "accept-eligible box: confirmed onboard Lock");
}

static void test_happy_path_geran() {
  tracking::Tracker tr;
  const uint32_t cid = static_cast<uint32_t>(actprove::ClassId::kGeran2);
  auto t = tr.seedFromGround(makeCue(cid, 10.0));
  expect(t.handoff_phase == actprove::HandoffPhase::kAwaitNose ||
             t.handoff_phase == actprove::HandoffPhase::kGroundSeeded,
         "geran: seed → AwaitNose/GroundSeeded");
  expect(!t.onboard_owns, "geran: seed not onboard_owns");
  expect(t.class_id == cid, "geran: seed class");
  expect(t.operator_lock_req, "geran: operator_lock_req");

  t = tr.update(makeNose(cid, 12.0, 1));
  expect(t.handoff_phase == actprove::HandoffPhase::kOnboard,
         "geran: handoff → Onboard");
  expect(t.onboard_owns, "geran: onboard_owns after handoff");
  expect(t.lock_state == actprove::LockState::kLock, "geran: lock after handoff");
  expect(t.cam_id == static_cast<uint8_t>(actprove::CamId::kEo),
         "geran: cam_id nose");
}

static void test_class_mismatch_blocks_handoff() {
  tracking::Tracker tr;
  const uint32_t seed = static_cast<uint32_t>(actprove::ClassId::kShahed136);
  const uint32_t other = static_cast<uint32_t>(actprove::ClassId::kGerbera);
  tr.seedFromGround(makeCue(seed, 1.0));
  auto t = tr.update(makeNose(other, 2.0, 1));
  expect(t.handoff_phase != actprove::HandoffPhase::kOnboard,
         "mismatch: no handoff on wrong class");
  expect(!t.onboard_owns, "mismatch: still not onboard_owns");
}

static void test_reject_legacy_quad_seed() {
  tracking::Tracker tr;
  auto t = tr.seedFromGround(
      makeCue(static_cast<uint32_t>(actprove::ClassId::kQuad), 1.0));
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "quad: seed rejected (not range-test class)");
  expect(t.track_id == 0, "quad: no track id");
}

static void test_reject_map_pin() {
  tracking::Tracker tr;
  auto cue = makeCue(static_cast<uint32_t>(actprove::ClassId::kGerbera), 1.0);
  cue.bbox_w = 0.f;
  cue.bbox_h = 0.f;
  auto t = tr.seedFromGround(cue);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "map-pin: rejected (no image box)");
}

static void test_bda_miss_reseed() {
  tracking::Tracker tr;
  const uint32_t cid = static_cast<uint32_t>(actprove::ClassId::kShahed136);
  tr.seedFromGround(makeCue(cid, 1.0));
  tr.update(makeNose(cid, 2.0, 1));
  auto t = tr.onBda(actprove::BdaMark::kMiss);
  expect(t.handoff_phase == actprove::HandoffPhase::kPostMiss,
         "bda miss: PostMiss");
  expect(!t.onboard_owns, "bda miss: cleared onboard_owns");
  t = tr.seedFromGround(makeCue(cid, 3.0));
  expect(t.handoff_phase == actprove::HandoffPhase::kAwaitNose ||
             t.handoff_phase == actprove::HandoffPhase::kGroundSeeded,
         "bda miss: re-seed without BOOT");
}

static void test_bda_destroyed_blocks_reseed() {
  tracking::Tracker tr;
  const uint32_t cid = static_cast<uint32_t>(actprove::ClassId::kGerbera);
  tr.seedFromGround(makeCue(cid, 1.0));
  tr.update(makeNose(cid, 2.0, 1));
  auto t = tr.onBda(actprove::BdaMark::kTargetDestroyed);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "destroyed: Idle");
  auto blocked = tr.seedFromGround(makeCue(cid, 4.0));
  expect(blocked.track_id == 0 &&
             blocked.handoff_phase == actprove::HandoffPhase::kIdle,
         "destroyed: re-seed blocked until reset");
  tr.reset();
  auto again = tr.seedFromGround(makeCue(cid, 5.0));
  expect(again.handoff_phase != actprove::HandoffPhase::kIdle,
         "destroyed: reset allows new WORK seed");
}

static void test_all_three_classes() {
  for (uint32_t cid = 0; cid <= 2; ++cid) {
    tracking::Tracker tr;
    tr.seedFromGround(makeCue(cid, 1.0));
    auto t = tr.update(makeNose(cid, 2.0, 1));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "class %u handoff Onboard", cid);
    expect(t.handoff_phase == actprove::HandoffPhase::kOnboard && t.onboard_owns,
           buf);
  }
}

static void test_ground_cam_id_detection_path() {
  tracking::Tracker tr;
  actprove::DetectionMsg g{};
  g.t_pps = 1.0;
  g.seq = 1;
  g.cam_id = static_cast<uint8_t>(actprove::CamId::kGroundEo);
  g.src_w = 640;
  g.src_h = 480;
  g.n = 1;
  g.boxes[0] = {100.f, 100.f, 40.f, 30.f, 1.0f,
                static_cast<uint32_t>(actprove::ClassId::kShahed136)};
  auto t = tr.update(g);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle && t.track_id == 0,
         "cam_id=2 detector output cannot impersonate operator Lock");
}

static void test_ground_operator_and_freshness_gates() {
  tracking::Tracker tr;
  auto cue = makeCue(static_cast<uint32_t>(actprove::ClassId::kGeran2), 1.0);
  cue.operator_lock_request = false;
  auto t = tr.seedFromGround(cue);
  expect(t.handoff_phase == actprove::HandoffPhase::kIdle,
         "ground cue without operator request is rejected");

  cue.operator_lock_request = true;
  tr.seedFromGround(cue);
  t = tr.update(makeNose(cue.class_id, 5.1, 1));
  expect(t.handoff_phase != actprove::HandoffPhase::kOnboard,
         "stale nose detection cannot complete handoff");
}

int main() {
  std::printf("=== Tracking virtual tests ===\n");
  test_lock_acceptance_scores();
  test_happy_path_geran();
  test_class_mismatch_blocks_handoff();
  test_reject_legacy_quad_seed();
  test_reject_map_pin();
  test_bda_miss_reseed();
  test_bda_destroyed_blocks_reseed();
  test_all_three_classes();
  test_ground_cam_id_detection_path();
  test_ground_operator_and_freshness_gates();
  std::printf("=== %s (%d failed) ===\n", g_fail ? "FAILED" : "ALL PASS",
              g_fail);
  return g_fail ? 1 : 0;
}
