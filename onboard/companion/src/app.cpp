#include "companion/app.hpp"

#include "mavlink_v1_cmds.h"

#include <algorithm>
#include <cmath>

namespace actprove::companion {
namespace {

bool track_ok_for_close(const actprove::TrackMsg& t) {
  return t.onboard_owns &&
         (t.lock_state == actprove::LockState::kLock ||
          t.lock_state == actprove::LockState::kCoast) &&
         t.lock_quality >= 0.45f &&
         t.bbox_w > 1.f && t.bbox_h > 1.f;
}

}  // namespace

CompanionApp::CompanionApp(CompanionConfig cfg) : cfg_(std::move(cfg)) {
  tracking::TrackerConfig tcfg;
  tcfg.accept.allow_any_class = cfg_.allow_any_class;
  tcfg.accept.min_conf = cfg_.min_conf;
  tcfg.accept.associate_conf_min = std::min(0.20f, cfg_.min_conf);
  tcfg.accept.confirm_hits = cfg_.confirm_hits;
  tcfg.accept.lock_quality_min = 0.55f;
  tcfg.enforce_range_test_classes = !cfg_.allow_any_class;
  tcfg.t_coast_s = cfg_.t_coast_s;
  tcfg.max_center_jump_px = 320.f;
  tracker_ = tracking::Tracker(tcfg);
}

void CompanionApp::reset() {
  tracker_.reset();
  rc_ = {};
  status_ = {};
  src_w_ = 0;
  src_h_ = 0;
  homing_ = {};
}

actprove::DetectionMsg CompanionApp::maybe_center(
    const actprove::DetectionMsg& det) const {
  if (!cfg_.prefer_center || det.n <= 1 || det.src_w == 0 || det.src_h == 0) {
    return det;
  }
  const float cx = 0.5f * static_cast<float>(det.src_w);
  const float cy = 0.5f * static_cast<float>(det.src_h);
  int best = -1;
  float best_d2 = 1e12f;
  const uint32_t n = std::min(det.n, actprove::kMaxDet);
  for (uint32_t i = 0; i < n; ++i) {
    const auto& b = det.boxes[i];
    if (b.conf < cfg_.min_conf) continue;
    if (b.w < 6.f || b.h < 6.f) continue;
    const float u = b.x + 0.5f * b.w;
    const float v = b.y + 0.5f * b.h;
    if (v > 0.88f * static_cast<float>(det.src_h)) continue;  // ground band
    const float du = u - cx;
    const float dv = v - cy;
    const float d2 = du * du + dv * dv;
    if (d2 < best_d2) {
      best_d2 = d2;
      best = static_cast<int>(i);
    }
  }
  if (best < 0) return det;
  actprove::DetectionMsg out = det;
  out.n = 1;
  out.boxes[0] = det.boxes[static_cast<uint32_t>(best)];
  return out;
}

void CompanionApp::on_detection(const actprove::DetectionMsg& det) {
  src_w_ = det.src_w;
  src_h_ = det.src_h;
  tracker_.update(maybe_center(det));
}

void CompanionApp::on_rc(const uint16_t* ch, uint8_t n) {
  (void)update_rc_gate(rc_, ch, n, cfg_.rc);
}

void CompanionApp::on_fc_heartbeat(bool armed, uint32_t custom_mode, double now_s) {
  status_.armed = armed;
  fc_mode_ = custom_mode;
  last_fc_hb_s_ = now_s;
}

void CompanionApp::tick(double now_s) {
  const actprove::TrackMsg& tr = tracker_.track();
  status_.intent = rc_.last;
  status_.lock_latched = rc_.lock_latched;
  status_.track_id = tr.track_id;
  status_.lock_quality = tr.lock_quality;
  status_.track_ok = track_ok_for_close(tr);

  const bool lost_link =
      last_fc_hb_s_ < 0.0 || (now_s - last_fc_hb_s_) > cfg_.lost_link_s;

  status_.want_rtl = (rc_.last == RcIntent::Abort) ||
                     (rc_.lock_latched && lost_link);
  status_.want_manual = (rc_.last == RcIntent::Takeover) ||
                        (rc_.last == RcIntent::Abort) || !rc_.lock_latched;

  const bool can_guide = rc_.lock_latched && !status_.want_manual &&
                         (!cfg_.require_armed || status_.armed) && !lost_link;
  status_.want_guided = can_guide;

  homing_ = {};
  status_.setpoint_valid = false;
  status_.vx = status_.vy = status_.vz = 0.f;
  status_.range_est_m = 0.f;

  if (!can_guide) {
    status_.phase = CompanionPhase::Manual;
    status_.mission_state = 0;  // BOOT / manual
    if (rc_.last == RcIntent::Abort) status_.mission_state = 5;  // RTB
    return;
  }

  if (status_.track_ok) {
    homing_ = image_homing(tr, src_w_, src_h_, cfg_.homing);
    status_.setpoint_valid = homing_.valid;
    status_.vx = homing_.vx;
    status_.vy = homing_.vy;
    status_.vz = homing_.vz;
    status_.range_est_m = homing_.range_est_m;
    status_.phase = CompanionPhase::Close;
    status_.mission_state = 3;  // CLOSE
  } else {
    status_.phase = CompanionPhase::Search;
    status_.mission_state = 1;  // SEARCH
  }
}

mavlink_bridge::LocalNedSetpoint CompanionApp::setpoint() const {
  mavlink_bridge::LocalNedSetpoint sp;
  if (!status_.setpoint_valid) return sp;
  sp.vx = status_.vx;
  sp.vy = status_.vy;
  sp.vz = status_.vz;
  sp.type_mask = AP_TYPEMASK_BODY_VEL;
  sp.coordinate_frame = AP_MAV_FRAME_BODY_NED;
  return sp;
}

}  // namespace actprove::companion
