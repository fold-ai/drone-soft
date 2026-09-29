#pragma once
// RC Lock / Takeover / Abort. Channel numbers are 1-based (CH7 = 7).
// Takeover always wins. Lock is software-latched until Takeover or Abort
// so a momentary switch still works.

#include <cstdint>

namespace actprove::companion {

struct RcGateConfig {
  int lock_ch{7};
  int takeover_ch{8};
  int abort_ch{0};  // 0 = unused
  uint16_t high_pwm{1700};
};

enum class RcIntent : uint8_t {
  Manual = 0,
  LockEngage = 1,  // rising edge, latch set
  LockHold = 2,
  Takeover = 3,
  Abort = 4,
};

struct RcGateState {
  bool lock_latched{false};
  bool lock_was_high{false};
  RcIntent last{RcIntent::Manual};
};

inline bool channel_high(const uint16_t* ch, uint8_t n, int one_based,
                         uint16_t high_pwm) {
  if (one_based < 1 || one_based > static_cast<int>(n)) return false;
  const uint16_t pwm = ch[one_based - 1];
  return pwm >= high_pwm && pwm < 2500;
}

RcIntent update_rc_gate(RcGateState& st, const uint16_t* ch, uint8_t n,
                        const RcGateConfig& cfg);

}  // namespace actprove::companion
