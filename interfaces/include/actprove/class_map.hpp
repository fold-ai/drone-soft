#pragma once
// Exported class map mirror (vision/training/exports/class_map_range_v1.json).
// Tracking Lock gate — keep in sync via export_class_map.py.

#include <cstdint>
#include <string_view>

namespace actprove {

struct ClassMapV1 {
  static constexpr int kNc = 3;
  static constexpr uint32_t kGate[3] = {0, 1, 2};

  static constexpr std::string_view nameOf(uint32_t id) {
    switch (id) {
      case 0:
        return "shahed_136";
      case 1:
        return "geran_2";
      case 2:
        return "gerbera";
      case 10:
        return "quad";
      case 11:
        return "fixed_wing";
      default:
        return "unknown";
    }
  }

  static constexpr bool inGate(uint32_t class_id) {
    return class_id == 0 || class_id == 1 || class_id == 2;
  }
};

// Lock acceptance vs detector scores (lock_acceptance_v1.yaml defaults).
struct LockAcceptance {
  float min_conf{0.45f};
  float min_box_area_px{64.f};
  float min_box_side_px{6.f};
  float max_box_area_frac{0.85f};
  float associate_conf_min{0.25f};
  float handoff_conf_min{0.30f};
  float lock_quality_min{0.6f};
  uint32_t confirm_hits{3};
  // First Orin intercept: operator already pointed the nose. Until a
  // test_drone engine exists, accept any class that passes geometry/conf.
  bool allow_any_class{false};

  bool classAllowed(uint32_t class_id) const {
    if (allow_any_class) {
      return true;
    }
    return ClassMapV1::inGate(class_id);
  }

  bool geometryOk(float w, float h, uint16_t src_w, uint16_t src_h) const {
    if (w < min_box_side_px || h < min_box_side_px) {
      return false;
    }
    const float area = w * h;
    if (area < min_box_area_px) {
      return false;
    }
    if (src_w > 0 && src_h > 0) {
      const float frac = area / (static_cast<float>(src_w) * static_cast<float>(src_h));
      if (frac > max_box_area_frac) {
        return false;
      }
    }
    return true;
  }

  /** Auto-detector box eligible to seed / confirm Lock. */
  bool acceptAutoBox(uint32_t class_id, float conf, float w, float h,
                     uint16_t src_w, uint16_t src_h) const {
    if (!classAllowed(class_id)) {
      return false;
    }
    if (conf < min_conf) {
      return false;
    }
    return geometryOk(w, h, src_w, src_h);
  }

  /** Manual / Ground EO: conf forced to 1.0; still need class + geometry. */
  bool acceptManualBox(uint32_t class_id, float w, float h, uint16_t src_w,
                       uint16_t src_h) const {
    if (!classAllowed(class_id)) {
      return false;
    }
    if (w <= 1.f || h <= 1.f) {
      return false;
    }
    return geometryOk(w, h, src_w, src_h);
  }
};

}  // namespace actprove
