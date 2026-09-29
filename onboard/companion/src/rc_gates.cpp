#include "companion/rc_gates.hpp"

namespace actprove::companion {

RcIntent update_rc_gate(RcGateState& st, const uint16_t* ch, uint8_t n,
                        const RcGateConfig& cfg) {
  if (!ch || n == 0) {
    st.last = st.lock_latched ? RcIntent::LockHold : RcIntent::Manual;
    return st.last;
  }

  const bool takeover = channel_high(ch, n, cfg.takeover_ch, cfg.high_pwm);
  const bool abort = cfg.abort_ch > 0 &&
                     channel_high(ch, n, cfg.abort_ch, cfg.high_pwm);
  const bool lock_high = channel_high(ch, n, cfg.lock_ch, cfg.high_pwm);

  if (takeover || abort) {
    st.lock_latched = false;
    st.lock_was_high = lock_high;
    st.last = takeover ? RcIntent::Takeover : RcIntent::Abort;
    return st.last;
  }

  if (lock_high && !st.lock_was_high) {
    st.lock_latched = true;
    st.lock_was_high = true;
    st.last = RcIntent::LockEngage;
    return st.last;
  }

  st.lock_was_high = lock_high;
  if (st.lock_latched) {
    st.last = RcIntent::LockHold;
    return st.last;
  }
  st.last = RcIntent::Manual;
  return st.last;
}

}  // namespace actprove::companion
