#pragma once
// Ground / operator cue acceptance for Navigation (no radar V1).
// Bearing-only is valid until nose EO acquires a track.
// Spec: docs/notes/GROUND_EO_CUE_AND_BDA.md + navigation/docs/ground_cue_and_bda.md

#include <cmath>
#include <cstdint>

namespace navigation {

enum class CueSource : uint8_t {
  kUnknown = 0,
  kGroundEo = 1,
  kNoseEo = 2,
  kOperator = 3,
};

struct GroundCue {
  double t_pps = 0.0;
  float bearing_az_ned_rad = 0.f;
  float bearing_el_ned_rad = 0.f;  // may be 0 if unknown
  float range_est_m = NAN;         // NaN = bearing-only (ACCEPTED)
  CueSource source = CueSource::kUnknown;
  float cue_quality = 0.f;         // 0..1
  bool has_image_box = false;      // map-pin alone → reject
};

struct CueAcceptConfig {
  float cue_quality_min = 0.3f;
  float cue_coast_s = 3.f;
  bool allow_bearing_only = true;  // V1: true (no radar)
};

inline bool cue_is_bearing_only(const GroundCue& c) {
  return !std::isfinite(c.range_est_m) || c.range_est_m <= 0.f;
}

// Nav acceptance: bearing-only OK; map-pin-only (no box / no bearing quality) NOT OK.
inline bool cue_acceptable(const GroundCue& c, const CueAcceptConfig& cfg = {}) {
  if (c.source == CueSource::kUnknown) return false;
  if (c.cue_quality < cfg.cue_quality_min) return false;
  // Ground/operator cue must come from an image box path (Safety freeze).
  if ((c.source == CueSource::kGroundEo || c.source == CueSource::kOperator) &&
      !c.has_image_box) {
    return false;
  }
  if (!cfg.allow_bearing_only && cue_is_bearing_only(c)) return false;
  // Azimuth always required for heading bias.
  if (!std::isfinite(c.bearing_az_ned_rad)) return false;
  return true;
}

}  // namespace navigation
