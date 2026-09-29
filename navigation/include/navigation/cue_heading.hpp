#pragma once
// Bearing-only cue → heading bias setpoints (pre-nose-acquire / re-cue).
// Owner: Navigation.

#include "navigation/ground_cue.hpp"
#include "navigation/heading_guidance.hpp"
#include "navigation/mavlink_setpoint.hpp"

namespace navigation {

struct CueHeadingConfig {
  float bias_speed_mps = 20.f;
  CueAcceptConfig accept{};
};

class CueHeadingBias {
 public:
  explicit CueHeadingBias(CueHeadingConfig cfg = {}) : cfg_(cfg) {}

  // Returns invalid setpoint if cue not acceptable.
  VelYawSetpoint update(const GroundCue& cue, const OwnshipState& own) const;

 private:
  CueHeadingConfig cfg_;
};

}  // namespace navigation
