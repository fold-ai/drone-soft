#include "tracking/tracker.hpp"

#include <cstdio>

static const char* phaseName(actprove::HandoffPhase p) {
  switch (p) {
    case actprove::HandoffPhase::kIdle:
      return "Idle";
    case actprove::HandoffPhase::kGroundSeeded:
      return "GroundSeeded";
    case actprove::HandoffPhase::kAwaitNose:
      return "AwaitNose";
    case actprove::HandoffPhase::kOnboard:
      return "Onboard";
    case actprove::HandoffPhase::kPostMiss:
      return "PostMiss";
    default:
      return "?";
  }
}

int main() {
  tracking::Tracker tr;

  // 1) Operator Lock on Ground EO
  actprove::GroundCueMsg cue{};
  cue.t_pps = 10.0;
  cue.u = 320.f;
  cue.v = 240.f;
  cue.bbox_w = 40.f;
  cue.bbox_h = 30.f;
  cue.src_w = 640;
  cue.src_h = 480;
  cue.class_id = 1;  // geran_2
  cue.operator_lock_request = true;
  auto t = tr.seedFromGround(cue);
  std::printf("seed phase=%s onboard=%d lock=%u\n", phaseName(t.handoff_phase),
              static_cast<int>(t.onboard_owns),
              static_cast<unsigned>(t.lock_state));

  // 2) Nose EO acquires same class → handoff
  actprove::DetectionMsg nose{};
  nose.t_pps = 12.0;
  nose.seq = 1;
  nose.cam_id = static_cast<uint8_t>(actprove::CamId::kEo);
  nose.src_w = 1920;
  nose.src_h = 1200;
  nose.n = 1;
  nose.boxes[0] = {900.f, 500.f, 50.f, 40.f, 0.85f, 1};
  t = tr.update(nose);
  std::printf("handoff phase=%s onboard=%d uv=(%.0f,%.0f)\n",
              phaseName(t.handoff_phase), static_cast<int>(t.onboard_owns), t.u,
              t.v);

  // 3) BDA MISS → re-seed allowed
  t = tr.onBda(actprove::BdaMark::kMiss);
  std::printf("bda_miss phase=%s reseed_next=%d\n", phaseName(t.handoff_phase),
              1);
  cue.t_pps = 20.0;
  t = tr.seedFromGround(cue);
  std::printf("reseed phase=%s\n", phaseName(t.handoff_phase));

  // 4) BDA destroyed → no re-seed
  tr.onBda(actprove::BdaMark::kMiss);
  nose.t_pps = 21.0;
  tr.update(nose);
  t = tr.onBda(actprove::BdaMark::kTargetDestroyed);
  auto blocked = tr.seedFromGround(cue);
  std::printf("destroyed phase=%s seed_blocked_id=%u\n",
              phaseName(t.handoff_phase), blocked.track_id);
  return 0;
}
