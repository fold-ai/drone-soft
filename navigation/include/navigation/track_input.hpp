#pragma once
// Track → Navigation ICD. Measurement epoch is always t_pps (PPS domain).
// Owner: Navigation (+ Tracking). No kill fields.

#include <actprove/detection_msg.hpp>
#include <actprove/track_msg.hpp>

namespace navigation {

using TrackState = actprove::TrackState;
using TrackMsg = actprove::TrackMsg;

inline bool track_usable_for_close(const TrackMsg& t) {
  return (t.state == TrackState::kConfirmed || t.state == TrackState::kCoast) &&
         t.onboard_owns &&
         t.cam_id == static_cast<uint8_t>(actprove::CamId::kEo) &&
         t.lock_quality >= 0.5f;
}

}  // namespace navigation
