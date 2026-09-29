#include "companion/rc_gates.hpp"

#include <cassert>
#include <cstdio>

int main() {
  using namespace actprove::companion;
  RcGateConfig cfg;
  RcGateState st;
  uint16_t ch[12];
  for (auto& c : ch) c = 1500;

  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::Manual);

  ch[6] = 1900;  // CH7 lock rising
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::LockEngage);
  assert(st.lock_latched);
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::LockHold);

  ch[6] = 1100;  // momentary release keeps latch
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::LockHold);

  ch[7] = 1900;  // CH8 takeover always wins
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::Takeover);
  assert(!st.lock_latched);

  ch[7] = 1100;
  ch[6] = 1900;
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::LockEngage);

  ch[7] = 1900;
  ch[5] = 1900;
  cfg.abort_ch = 6;
  assert(update_rc_gate(st, ch, 12, cfg) == RcIntent::Takeover);

  std::puts("test_rc_gates PASS");
  return 0;
}
