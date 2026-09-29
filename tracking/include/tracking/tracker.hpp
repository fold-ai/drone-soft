#pragma once
// Single-primary tracker + Ground EO → nose EO handoff.
// Lock acceptance from detector scores: class gate, min conf, min box area.
// Consumes exported class map (actprove::ClassMapV1 / LockAcceptance).
// No PWM. English comments.

#include "tracking/detection_msg.hpp"
#include "tracking/ground_cue_msg.hpp"
#include "tracking/lock_acceptance.hpp"
#include "tracking/track_msg.hpp"

#include <optional>

namespace tracking {

struct TrackerConfig {
  actprove::LockAcceptance accept{};
  float iou_gate{0.1f};
  float t_coast_s{0.5f};
  float max_center_jump_px{200.f};
  float max_handoff_age_s{3.f};
  // Prefer enforce via accept.classAllowed / ClassMapV1.
  bool enforce_range_test_classes{true};

  float conf_min() const { return accept.associate_conf_min; }
  float lock_quality_min() const { return accept.lock_quality_min; }
  uint32_t confirm_hits() const { return accept.confirm_hits; }
  float handoff_conf_min() const { return accept.handoff_conf_min; }
};

class Tracker {
 public:
  explicit Tracker(TrackerConfig cfg = {});

  actprove::TrackMsg update(const actprove::DetectionMsg& det);
  actprove::TrackMsg seedFromGround(const actprove::GroundCueMsg& cue);
  actprove::TrackMsg onBda(actprove::BdaMark mark);

  const actprove::TrackMsg& track() const { return track_; }
  actprove::LockState lock_state() const { return track_.lock_state; }
  actprove::HandoffPhase handoff_phase() const {
    return track_.handoff_phase;
  }
  bool onboardOwns() const { return track_.onboard_owns; }
  const actprove::LockAcceptance& acceptance() const { return cfg_.accept; }

  void reset();

 private:
  TrackerConfig cfg_;
  actprove::TrackMsg track_{};
  uint32_t next_id_{1};
  double last_meas_t_pps_{0.0};
  uint32_t hits_{0};
  uint32_t misses_{0};
  uint32_t seed_class_{0};
  bool reseed_allowed_{true};

  bool classAllowed(uint32_t class_id) const;
  bool boxAcceptAuto(const actprove::Box& b, uint16_t src_w,
                     uint16_t src_h) const;
  static float iou(const actprove::Box& a, const actprove::Box& b);
  std::optional<actprove::Box> pickPrimary(const actprove::DetectionMsg& det,
                                           bool prefer_seed_class,
                                           bool require_lock_accept) const;
  void applyNoseMeasurement(const actprove::DetectionMsg& det,
                            const actprove::Box& box);
  void coast(double t_pps);
  void recomputeLock();
  void enterAwaitNose();
  bool tryHandoffFromNose(const actprove::DetectionMsg& det);
};

}  // namespace tracking
