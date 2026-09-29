#include "safety_gates/state_machine.hpp"

namespace actprove::safety {

SmOutputs StateMachine::step(const SmInputs& in, std::optional<Cmd> cmd) {
  if (out_.fts_latched || out_.state == MissionState::FTS) {
    out_.state = MissionState::FTS;
    out_.allow_heading_cmd = false;
    out_.fts_latched = true;
    out_.reason = "FTS latched";
    return out_;
  }

  if (in.hard_fail || (cmd && *cmd == Cmd::FTS)) {
    out_.state = MissionState::FTS;
    out_.allow_heading_cmd = false;
    out_.fts_latched = true;
    out_.reason = in.hard_fail ? "hard_fail" : "FTS cmd";
    return out_;
  }

  if (in.orin_hb_age_s >= SmInputs::kLostLinkRtbS) {
    out_.state = MissionState::RTB;
    out_.allow_heading_cmd = false;
    out_.reason = "lost-link 3s -> RTB";
    return out_;
  }

  if (cmd && *cmd == Cmd::RTB) {
    out_.state = MissionState::RTB;
    out_.allow_heading_cmd = false;
    out_.reason = "RTB cmd";
    return out_;
  }

  if (cmd && *cmd == Cmd::ABORT) {
    out_.state = MissionState::ABORT;
    out_.allow_heading_cmd = false;
    abort_cleared_for_work_ = false;
    out_.reason = "ABORT cmd";
    return out_;
  }

  switch (out_.state) {
    case MissionState::BOOT:
      out_.allow_heading_cmd = false;
      if (!in.bit_ok) {
        out_.reason = "BIT not OK — hold ground / disarm";
        break;
      }
      if (cmd && *cmd == Cmd::WORK && abort_cleared_for_work_) {
        out_.state = MissionState::SEARCH;
        out_.reason = "WORK -> SEARCH";
      }
      break;

    case MissionState::SEARCH:
      out_.allow_heading_cmd = false;
      if (!in.engagement_corridor) {
        out_.reason = "no corridor — fail-HOLD loiter";
        break;
      }
      if (in.t_search_miss_s >= in.search_timeout_s) {
        out_.state = MissionState::ABORT;
        abort_cleared_for_work_ = false;
        out_.reason = "SEARCH timeout -> ABORT";
        break;
      }
      if (in.stable_track_box) {
        out_.state = MissionState::LOCK;
        out_.reason = "track+box -> LOCK";
      }
      break;

    case MissionState::LOCK:
      out_.allow_heading_cmd = false;
      if (in.lost_box_s >= in.t_coast_s) {
        out_.state = MissionState::SEARCH;
        out_.reason = "lost-box coast -> SEARCH";
        break;
      }
      if (in.geometry_ok_for_close) {
        out_.state = MissionState::CLOSE;
        out_.reason = "geometry OK -> CLOSE";
      }
      break;

    case MissionState::CLOSE:
      out_.allow_heading_cmd = true;
      if (in.miss_hold_done) {
        out_.state = MissionState::ABORT;
        abort_cleared_for_work_ = false;
        out_.reason = "miss-distance hold done -> ABORT";
        out_.allow_heading_cmd = false;
      } else if (in.lost_box_s >= in.t_coast_s) {
        out_.state = MissionState::SEARCH;
        out_.reason = "brief coast -> SEARCH";
        out_.allow_heading_cmd = false;
      }
      break;

    case MissionState::ABORT:
      out_.allow_heading_cmd = false;
      if (cmd && *cmd == Cmd::WORK) {
        abort_cleared_for_work_ = true;
        if (in.link_ok && in.energy_ok && in.bit_ok) {
          out_.state = MissionState::SEARCH;
          out_.reason = "ABORT clear + WORK -> SEARCH";
        } else {
          out_.reason = "WORK armed; waiting link+energy+BIT";
        }
      } else if (in.link_ok && in.energy_ok) {
        out_.reason = "ABORT hold — RTB available";
      } else {
        out_.reason = "ABORT loiter HOLD";
      }
      break;

    case MissionState::RTB:
      out_.allow_heading_cmd = false;
      out_.reason = "RTB in progress";
      break;

    case MissionState::FTS:
      break;
  }
  return out_;
}

}  // namespace actprove::safety
