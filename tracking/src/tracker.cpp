#include "tracking/tracker.hpp"

#include <algorithm>
#include <cmath>

namespace tracking {

namespace {
bool isNoseEo(uint8_t cam_id) {
  return cam_id == static_cast<uint8_t>(actprove::CamId::kEo);
}
bool isGroundEo(uint8_t cam_id) {
  return cam_id == static_cast<uint8_t>(actprove::CamId::kGroundEo);
}
}  // namespace

Tracker::Tracker(TrackerConfig cfg) : cfg_(std::move(cfg)) {}

void Tracker::reset() {
  track_ = {};
  hits_ = 0;
  misses_ = 0;
  last_meas_t_pps_ = 0.0;
  seed_class_ = 0;
  reseed_allowed_ = true;
}

bool Tracker::classAllowed(uint32_t class_id) const {
  if (!cfg_.enforce_range_test_classes) {
    return true;
  }
  return cfg_.accept.classAllowed(class_id);
}

bool Tracker::boxAcceptAuto(const actprove::Box& b, uint16_t src_w,
                            uint16_t src_h) const {
  return cfg_.accept.acceptAutoBox(b.class_id, b.conf, b.w, b.h, src_w, src_h);
}

float Tracker::iou(const actprove::Box& a, const actprove::Box& b) {
  const float ax2 = a.x + a.w;
  const float ay2 = a.y + a.h;
  const float bx2 = b.x + b.w;
  const float by2 = b.y + b.h;
  const float ix1 = std::max(a.x, b.x);
  const float iy1 = std::max(a.y, b.y);
  const float ix2 = std::min(ax2, bx2);
  const float iy2 = std::min(ay2, by2);
  const float iw = std::max(0.f, ix2 - ix1);
  const float ih = std::max(0.f, iy2 - iy1);
  const float inter = iw * ih;
  const float uni = a.w * a.h + b.w * b.h - inter;
  return uni > 0.f ? inter / uni : 0.f;
}

std::optional<actprove::Box> Tracker::pickPrimary(
    const actprove::DetectionMsg& det, bool prefer_seed_class,
    bool require_lock_accept) const {
  if (det.n == 0) {
    return std::nullopt;
  }
  const uint32_t n = std::min(det.n, actprove::kMaxDet);
  const float assoc_floor = cfg_.conf_min();

  auto eligible = [&](const actprove::Box& b) {
    if (prefer_seed_class && b.class_id != seed_class_) {
      return false;
    }
    if (require_lock_accept) {
      return boxAcceptAuto(b, det.src_w, det.src_h);
    }
    if (b.conf < assoc_floor) {
      return false;
    }
    return classAllowed(b.class_id);
  };

  if (track_.state == actprove::TrackState::kConfirmed ||
      track_.state == actprove::TrackState::kCoast ||
      track_.state == actprove::TrackState::kTentative) {
    actprove::Box pred{};
    pred.x = track_.u - 0.5f * track_.bbox_w;
    pred.y = track_.v - 0.5f * track_.bbox_h;
    pred.w = track_.bbox_w;
    pred.h = track_.bbox_h;

    int best_i = -1;
    float best_iou = cfg_.iou_gate;
    for (uint32_t i = 0; i < n; ++i) {
      const auto& b = det.boxes[i];
      if (!eligible(b)) {
        continue;
      }
      const float s = iou(pred, b);
      if (s >= best_iou) {
        best_iou = s;
        best_i = static_cast<int>(i);
      }
    }
    if (best_i >= 0) {
      return det.boxes[static_cast<uint32_t>(best_i)];
    }

    float best_d2 = cfg_.max_center_jump_px * cfg_.max_center_jump_px;
    best_i = -1;
    for (uint32_t i = 0; i < n; ++i) {
      const auto& b = det.boxes[i];
      if (!eligible(b)) {
        continue;
      }
      const float cu = b.x + 0.5f * b.w;
      const float cv = b.y + 0.5f * b.h;
      const float du = cu - track_.u;
      const float dv = cv - track_.v;
      const float d2 = du * du + dv * dv;
      if (d2 <= best_d2) {
        best_d2 = d2;
        best_i = static_cast<int>(i);
      }
    }
    if (best_i >= 0) {
      return det.boxes[static_cast<uint32_t>(best_i)];
    }
  }

  int best_i = -1;
  float best_c = require_lock_accept ? cfg_.accept.min_conf : assoc_floor;
  for (uint32_t i = 0; i < n; ++i) {
    const auto& b = det.boxes[i];
    if (!eligible(b)) {
      continue;
    }
    if (b.conf >= best_c) {
      best_c = b.conf;
      best_i = static_cast<int>(i);
    }
  }
  if (best_i < 0) {
    return std::nullopt;
  }
  return det.boxes[static_cast<uint32_t>(best_i)];
}

void Tracker::enterAwaitNose() {
  track_.handoff_phase = actprove::HandoffPhase::kAwaitNose;
  track_.onboard_owns = false;
  track_.lock_state = actprove::LockState::kLock;
  track_.lock_quality = std::max(track_.lock_quality, 0.55f);
}

actprove::TrackMsg Tracker::seedFromGround(const actprove::GroundCueMsg& cue) {
  if (!reseed_allowed_) {
    return track_;
  }
  if (!cue.operator_lock_request) {
    return track_;
  }
  if (!cfg_.accept.acceptManualBox(cue.class_id, cue.bbox_w, cue.bbox_h,
                                   cue.src_w, cue.src_h)) {
    return track_;
  }

  const bool fresh =
      track_.handoff_phase == actprove::HandoffPhase::kIdle ||
      track_.handoff_phase == actprove::HandoffPhase::kPostMiss ||
      track_.state == actprove::TrackState::kNone ||
      track_.state == actprove::TrackState::kLost;

  if (fresh) {
    track_.track_id = next_id_++;
    hits_ = 1;
  } else {
    ++hits_;
  }

  seed_class_ = cue.class_id;
  track_.set_measurement_epoch(cue.t_pps);
  track_.t_publish_pps = cue.t_pps;
  track_.cam_id = static_cast<uint8_t>(actprove::CamId::kGroundEo);
  track_.cue_source = track_.cam_id;
  track_.u = cue.u;
  track_.v = cue.v;
  track_.bbox_w = cue.bbox_w;
  track_.bbox_h = cue.bbox_h;
  track_.class_id = cue.class_id;
  track_.bearing_ned_rad[0] = cue.bearing_ned_rad[0];
  track_.bearing_ned_rad[1] = cue.bearing_ned_rad[1];
  track_.operator_lock_req = cue.operator_lock_request;
  track_.time_since_meas_s = 0.f;
  track_.age_updates = hits_;
  track_.state = actprove::TrackState::kConfirmed;
  hits_ = std::max(hits_, cfg_.confirm_hits());
  last_meas_t_pps_ = cue.t_pps;
  misses_ = 0;

  track_.handoff_phase = actprove::HandoffPhase::kGroundSeeded;
  track_.onboard_owns = false;
  track_.lock_quality = 0.75f;
  track_.lock_state = actprove::LockState::kLock;
  enterAwaitNose();
  return track_;
}

bool Tracker::tryHandoffFromNose(const actprove::DetectionMsg& det) {
  if (!isNoseEo(det.cam_id)) {
    return false;
  }
  if (track_.handoff_phase != actprove::HandoffPhase::kGroundSeeded &&
      track_.handoff_phase != actprove::HandoffPhase::kAwaitNose) {
    return false;
  }
  if (!classAllowed(seed_class_)) {
    return false;
  }
  const double cue_age_s = det.t_pps - last_meas_t_pps_;
  if (!std::isfinite(det.t_pps) || !std::isfinite(last_meas_t_pps_) ||
      cue_age_s < 0.0 || cue_age_s > cfg_.max_handoff_age_s) {
    return false;
  }
  const uint32_t n = std::min(det.n, actprove::kMaxDet);
  int best_i = -1;
  float best_c = cfg_.handoff_conf_min();
  for (uint32_t i = 0; i < n; ++i) {
    const auto& b = det.boxes[i];
    if (b.class_id != seed_class_) {
      continue;
    }
    // Handoff: conf floor + geometry (class already matched).
    if (b.conf < best_c) {
      continue;
    }
    if (!cfg_.accept.geometryOk(b.w, b.h, det.src_w, det.src_h)) {
      continue;
    }
    best_c = b.conf;
    best_i = static_cast<int>(i);
  }
  if (best_i < 0) {
    return false;
  }
  applyNoseMeasurement(det, det.boxes[static_cast<uint32_t>(best_i)]);
  track_.handoff_phase = actprove::HandoffPhase::kOnboard;
  track_.onboard_owns = true;
  track_.cue_source = static_cast<uint8_t>(actprove::CamId::kEo);
  track_.operator_lock_req = false;
  recomputeLock();
  return true;
}

void Tracker::applyNoseMeasurement(const actprove::DetectionMsg& det,
                                   const actprove::Box& box) {
  const bool new_track =
      track_.state == actprove::TrackState::kNone ||
      track_.state == actprove::TrackState::kLost;

  if (new_track) {
    track_.track_id = next_id_++;
    hits_ = 0;
  }

  track_.set_measurement_epoch(det.t_pps);
  track_.t_publish_pps = det.t_pps;
  track_.cam_id = det.cam_id;
  track_.u = box.x + 0.5f * box.w;
  track_.v = box.y + 0.5f * box.h;
  track_.bbox_w = box.w;
  track_.bbox_h = box.h;
  track_.class_id = box.class_id;
  seed_class_ = box.class_id;
  track_.time_since_meas_s = 0.f;
  last_meas_t_pps_ = det.t_pps;
  ++hits_;
  misses_ = 0;
  track_.age_updates = hits_;

  if (hits_ >= cfg_.confirm_hits()) {
    track_.state = actprove::TrackState::kConfirmed;
  } else {
    track_.state = actprove::TrackState::kTentative;
  }
}

void Tracker::coast(double t_pps) {
  if (track_.state == actprove::TrackState::kNone ||
      track_.state == actprove::TrackState::kLost) {
    track_.set_measurement_epoch(last_meas_t_pps_);
    track_.t_publish_pps = t_pps;
    track_.lock_state = actprove::LockState::kNoLock;
    track_.lock_quality = 0.f;
    track_.onboard_owns = false;
    return;
  }

  ++misses_;
  const float dt = static_cast<float>(t_pps - last_meas_t_pps_);
  track_.time_since_meas_s = std::max(0.f, dt);
  track_.t_publish_pps = t_pps;
  track_.set_measurement_epoch(last_meas_t_pps_);

  if (track_.time_since_meas_s <= cfg_.t_coast_s) {
    track_.state = actprove::TrackState::kCoast;
  } else {
    track_.state = actprove::TrackState::kLost;
    hits_ = 0;
    if (track_.handoff_phase == actprove::HandoffPhase::kOnboard) {
      track_.handoff_phase = actprove::HandoffPhase::kIdle;
    }
  }
  recomputeLock();
}

void Tracker::recomputeLock() {
  if (track_.handoff_phase == actprove::HandoffPhase::kGroundSeeded ||
      track_.handoff_phase == actprove::HandoffPhase::kAwaitNose) {
    track_.lock_state = actprove::LockState::kLock;
    track_.onboard_owns = false;
    return;
  }

  if (track_.state == actprove::TrackState::kConfirmed &&
      track_.handoff_phase == actprove::HandoffPhase::kOnboard) {
    float q = 0.7f;
    if (track_.bbox_w > 2.f && track_.bbox_h > 2.f) {
      q += 0.15f;
    }
    if (misses_ == 0) {
      q += 0.15f;
    }
    track_.lock_quality = std::min(1.f, q);
    track_.lock_state = (track_.lock_quality >= cfg_.lock_quality_min())
                            ? actprove::LockState::kLock
                            : actprove::LockState::kNoLock;
    track_.onboard_owns = track_.lock_state == actprove::LockState::kLock;
    return;
  }

  if (track_.state == actprove::TrackState::kCoast) {
    track_.lock_quality = std::max(
        0.f, 0.5f - 0.5f * (track_.time_since_meas_s / cfg_.t_coast_s));
    track_.lock_state = actprove::LockState::kCoast;
    return;
  }

  track_.lock_quality = 0.f;
  track_.lock_state = actprove::LockState::kNoLock;
  track_.onboard_owns = false;
}

actprove::TrackMsg Tracker::update(const actprove::DetectionMsg& det) {
  if (isGroundEo(det.cam_id)) {
    // DetectionMsg contains detector output, not operator authorization.
    // Only an explicit GroundCueMsg may seed a manual lock.
    return track_;
  }

  if (tryHandoffFromNose(det)) {
    return track_;
  }

  if (isNoseEo(det.cam_id) ||
      det.cam_id == static_cast<uint8_t>(actprove::CamId::kThermal)) {
    const bool class_gate =
        track_.handoff_phase == actprove::HandoffPhase::kOnboard;
    // New auto locks require accept thresholds; coast association uses floor.
    const bool require_accept =
        track_.handoff_phase == actprove::HandoffPhase::kIdle ||
        track_.handoff_phase == actprove::HandoffPhase::kPostMiss ||
        track_.state == actprove::TrackState::kNone ||
        track_.state == actprove::TrackState::kLost;
    const auto box =
        pickPrimary(det, class_gate, require_accept);
    if (box.has_value()) {
      applyNoseMeasurement(det, *box);
      if (track_.handoff_phase == actprove::HandoffPhase::kIdle ||
          track_.handoff_phase == actprove::HandoffPhase::kPostMiss) {
        track_.handoff_phase = actprove::HandoffPhase::kOnboard;
        track_.onboard_owns = true;
        track_.cue_source = det.cam_id;
      }
      recomputeLock();
    } else {
      coast(det.t_pps);
    }
    return track_;
  }

  coast(det.t_pps);
  return track_;
}

actprove::TrackMsg Tracker::onBda(actprove::BdaMark mark) {
  if (mark == actprove::BdaMark::kMiss) {
    track_.onboard_owns = false;
    track_.operator_lock_req = false;
    track_.lock_state = actprove::LockState::kNoLock;
    track_.lock_quality = 0.f;
    track_.state = actprove::TrackState::kLost;
    track_.handoff_phase = actprove::HandoffPhase::kPostMiss;
    reseed_allowed_ = true;
    hits_ = 0;
    misses_ = 0;
    return track_;
  }

  if (mark == actprove::BdaMark::kTargetDestroyed) {
    track_ = {};
    track_.handoff_phase = actprove::HandoffPhase::kIdle;
    reseed_allowed_ = false;
    hits_ = 0;
    misses_ = 0;
    seed_class_ = 0;
    last_meas_t_pps_ = 0.0;
    return track_;
  }

  return track_;
}

}  // namespace tracking
