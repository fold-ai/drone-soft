#pragma once
// GCS Ground EO operator Lock → Tracking seed / cue.
// Image box required — never map-pin alone (see MANUAL_LOCK + GROUND_EO specs).

#include <cstdint>
#include <limits>

namespace actprove {

struct GroundCueMsg {
  double t_pps{0.0};           // GCS / common clock stamp
  float u{0.f};                // box center in Ground EO pixels
  float v{0.f};
  float bbox_w{0.f};
  float bbox_h{0.f};
  uint16_t src_w{0};
  uint16_t src_h{0};
  uint32_t class_id{0};        // operator-confirmed type when known
  float bearing_ned_rad[2]{
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN()};  // optional SEARCH bias for Nav
  bool operator_lock_request{true};
  bool class_forced{false};    // operator overrode detector class
};

enum class BdaMark : uint8_t {
  kNone = 0,
  kMiss = 1,            // MISS / REATTACK — allow re-seed, no BOOT
  kTargetDestroyed = 2  // clear engagement; Safety → RTB
};

}  // namespace actprove
