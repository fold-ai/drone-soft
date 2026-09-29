#pragma once
// Perception / GCS → Tracking wire format (ICD: docs/icd/detection_msg.md)
// LOCKED. Correlate on t_pps only. No PWM / guidance on this path.

#include <cstdint>

#include <actprove/class_map.hpp>

namespace actprove {

struct Box {
  float x{0.f};
  float y{0.f};
  float w{0.f};
  float h{0.f};  // full-sensor pixels, top-left origin
  float conf{0.f};
  uint32_t class_id{0};  // ClassId range-test gate
};

static constexpr uint32_t kMaxDet = 32;

// V1 range-test Lock gate (GROUND_EO_CUE_AND_BDA.md)
enum class ClassId : uint32_t {
  kShahed136 = 0,
  kGeran2 = 1,
  kGerbera = 2,
  // Legacy / optional — NOT in V1 Lock gate
  kQuad = 10,
  kFixedWing = 11,
};

inline bool isRangeTestClass(uint32_t class_id) {
  return ClassMapV1::inGate(class_id);
}

enum class CamId : uint8_t {
  kEo = 0,        // Nose EO (Basler) — V1 primary
  kThermal = 1,   // Deferred V1.1
  kGroundEo = 2,  // GCS Ground EO cue / operator Lock
};

struct DetectionMsg {
  double t_pps{0.0};  // PPS domain, mid-exposure (or GCS stamp for ground)
  uint64_t seq{0};
  uint8_t cam_id{0};  // CamId
  uint16_t src_w{0};
  uint16_t src_h{0};
  uint32_t n{0};  // 0..kMaxDet; empty still means frame processed
  Box boxes[kMaxDet]{};
};

}  // namespace actprove
