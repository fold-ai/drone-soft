# Forbidden V1 behaviors (SAFETY)

Enforced in `mission_sm.c` comments / control flow:

1. **No combat-intercept state** — only BOOT…FTS (`ap_mission_state_t` 0–6).
2. **No terminal stick / kill / impact-seeking** — CLOSE is commanded-miss hold only; miss done → ABORT.
3. **No explosive FTS** — FTS asserts throttle idle + surfaces fixed + fuel-cut relay only.
4. **Never auto-FTS on lost-link** — `T_link` (3 s) → RTB only; FTS is commanded or `hard_fail`.
5. **No SEARCH without WORK + corridor** — BOOT/ABORT require WORK; corridor_loaded required.
6. **No new-target hunt without corridor** — SEARCH fail-HOLDs if corridor drops.
7. **No WORK in LOCK or CLOSE** — uplink WORK ignored in those states.
8. **FTS latched** — only `ap_mission_sm_reset()` (sim/bench power-cycle) exits FTS.
