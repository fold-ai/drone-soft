#pragma once
#include "safety_gates/mission_state.hpp"
#include <optional>
#include <string>

namespace actprove::safety {

struct SmInputs {
  bool bit_ok{false};
  bool engagement_corridor{false};
  bool stable_track_box{false};
  bool geometry_ok_for_close{false};
  bool miss_hold_done{false};
  bool link_ok{true};
  bool energy_ok{true};
  bool hard_fail{false};
  double lost_box_s{0.0};
  double t_coast_s{2.0};
  double t_search_miss_s{0.0};
  double search_timeout_s{60.0};
  double orin_hb_age_s{0.0};
  static constexpr double kLostLinkRtbS = 3.0;
};

struct SmOutputs {
  MissionState state{MissionState::BOOT};
  bool allow_heading_cmd{false};
  bool fts_latched{false};
  std::string reason;
};

class StateMachine {
 public:
  SmOutputs step(const SmInputs& in, std::optional<Cmd> cmd);
  MissionState state() const { return out_.state; }
  const SmOutputs& outputs() const { return out_; }
 private:
  SmOutputs out_{};
  bool abort_cleared_for_work_{true};
};

}  // namespace actprove::safety
