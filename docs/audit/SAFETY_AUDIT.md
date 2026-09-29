# SAFETY Audit — actprove-drone V1

| Field | Value |
|-------|-------|
| Date | **2026-09-16** (America/Chicago) |
| Airframe | N250 |
| Sensors | EO-only (no radar) |
| Mission | V1 see / hold / commanded miss — **not kill** |
| Scope | `onboard/safety_gates` mission SM + frozen uplink ICD |
| Sources | `mission_sm.h` / `mission_sm.c`, `tests/test_mission_sm.c`, `README.md`, `docs/forbidden_v1.md`, `interfaces/mavlink/mavlink_v1_cmds.h`, `interfaces/mavlink/mission_state.h`, `V1_PLAN.md` §6 |
| Auditor | Shared-box rebuild + code/test cross-check |

---

## Scope (N250 / EO-only / V1)

This audit covers the V1 **SAFETY mission state machine** and frozen C2 uplink names/macros for actprove-drone:

- States: BOOT, SEARCH, LOCK, CLOSE, ABORT, RTB, FTS (`ap_mission_state_t` 0–6).
- Mission intent: see → lock → commanded miss (hold / break-off). **Not** impact-seeking / kill.
- LAND_SOFT is an **ABORT submode flag** (`land_soft_active`), not enum 7.
- Out of scope for this unit audit: FC PX4 parameter trees, live RF link, guidance setpoint math outside SM, G1–G3 flight evidence packs, range personnel assignment (noted as residual).

---

## Build & virtual test results

**Command (2026-09-16):**

```bash
cd /workspace/actprove-drone/onboard/safety_gates
cmake -S . -B build && cmake --build build && ./build/test_mission_sm
```

| Result | Value |
|--------|-------|
| Build | OK (`libsafety_gates.a`, `test_mission_sm`) |
| Exit code | **0** |
| Summary | `ALL TESTS PASSED` |

### Full stdout

```
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /workspace/actprove-drone/onboard/safety_gates/build
[ 25%] Building C object CMakeFiles/safety_gates.dir/src/mission_sm.c.o
[ 50%] Linking C static library libsafety_gates.a
[ 50%] Built target safety_gates
[ 75%] Building C object CMakeFiles/test_mission_sm.dir/tests/test_mission_sm.c.o
[100%] Linking C executable test_mission_sm
[100%] Built target test_mission_sm
PASS boot stays without WORK → BOOT
PASS no SEARCH without corridor → BOOT
PASS WORK + corridor → SEARCH → SEARCH
PASS precondition SEARCH → SEARCH
PASS link loss < 3s stays → SEARCH
PASS lost-link → RTB → RTB
PASS lost-link does not assert FTS outputs
PASS track → LOCK → LOCK
PASS coast < T_coast → LOCK
PASS lost-box → SEARCH → SEARCH
PASS SEARCH timeout → ABORT → ABORT
PASS FTS cmd → FTS
PASS FTS outputs idle+surfaces+fuel-cut
PASS FTS stays latched → FTS
PASS reset → BOOT (sim power cycle) → BOOT
PASS in LOCK → LOCK
PASS WORK ignored in LOCK → LOCK
PASS geometry → CLOSE → CLOSE
PASS WORK ignored in CLOSE → CLOSE
PASS CLOSE → CLOSE
PASS miss hold → ABORT → ABORT
PASS miss hold without test_recover → no LAND_SOFT
PASS ABORT+WORK → SEARCH → SEARCH
PASS hard_fail → FTS → FTS
PASS BOOT BIT timeout → ABORT → ABORT
PASS RTB cmd → RTB
PASS WORK/ABORT ignored in RTB → RTB
PASS BOOT
PASS SEARCH
PASS LOCK
PASS CLOSE
PASS ABORT
PASS RTB
PASS FTS
PASS MISSION_STATE name
PASS AP_LOST_LINK_RTB_S == 3
PASS op-lock precondition SEARCH → SEARCH
PASS operator_lock → LOCK → LOCK
PASS reason operator_lock
PASS operator_lock ignored in BOOT → BOOT
PASS in SEARCH for ignore tests → SEARCH
PASS operator_lock ignored without box → SEARCH
PASS auto → LOCK → LOCK
PASS operator_lock ignored when not SEARCH → LOCK
PASS TEST_RECOVER SEARCH → SEARCH
PASS test_recover latched at SEARCH
PASS TEST_RECOVER CLOSE → CLOSE
PASS miss → ABORT LAND_SOFT → ABORT
PASS land_soft_active=1
PASS reason LAND_SOFT
PASS LAND_SOFT lost-link stays ABORT → ABORT
PASS land_soft still active after lost-link
PASS miss → plain ABORT → ABORT
PASS no LAND_SOFT
PASS TEST_RECOVER off lost-link → RTB → RTB
PASS precondition LAND_SOFT
PASS explicit RTB overrides LAND_SOFT → RTB
PASS land_soft cleared on RTB
PASS BDA destroyed precondition CLOSE → CLOSE
PASS TARGET_DESTROYED accepted
PASS TARGET_DESTROYED → RTB → RTB
PASS TARGET_DESTROYED land_soft_active=0
PASS TARGET_DESTROYED engagement cleared
PASS test_recover armed for BDA
PASS BDA+recover CLOSE → CLOSE
PASS BDA destroyed → RTB not LAND_SOFT → RTB
PASS BDA destroyed never LAND_SOFT
PASS MISS/REATTACK accepted
PASS MISS/REATTACK → ABORT → ABORT
PASS MISS/REATTACK engagement cleared
PASS MISS/REATTACK no LAND_SOFT
PASS MISS/REATTACK then WORK → SEARCH re-cue → SEARCH
PASS MISS/REATTACK stays ABORT not FTS → ABORT
PASS MISS/REATTACK not FTS
PASS MISS/REATTACK does not assert FTS outputs
PASS unknown cmd → -1
PASS AP_UPLINK_TARGET_DESTROYED==3
PASS AP_UPLINK_MISS_REATTACK==4
PASS NAME TARGET_DESTROYED
PASS NAME MISS/REATTACK
PASS bare MISS unknown
PASS bare REATTACK unknown

ALL TESTS PASSED
```

Test functions executed: `test_work_only_search_entry`, `test_lost_link_rtb`, `test_lost_track_search_abort`, `test_fts_latch`, `test_no_work_in_lock_close`, `test_miss_hold_and_hard_fail`, `test_rtb_and_names`, `test_operator_lock_ok`, `test_operator_lock_ignored`, `test_test_recover_land_soft`, `test_test_recover_off_lost_link`, `test_rtb_overrides_land_soft`, `test_bda_destroyed_rtb`, `test_bda_destroyed_not_land_soft`, `test_bda_miss_reattack_abort_recue`, `test_bda_miss_reattack_not_kill_fts`, `test_bda_unknown_and_macros`.

---

## Checklist

| Item | Expected | Verified in code / tests | Status |
|------|----------|---------------------------|--------|
| BDA `TARGET_DESTROYED` | USER_1 p1=3; string `TARGET_DESTROYED` → **RTB always**; engagement cleared; never LAND_SOFT | `mavlink_v1_cmds.h` `AP_UPLINK_TARGET_DESTROYED==3`; `mission_sm.c` handle → RTB; tests `TARGET_DESTROYED → RTB`, `TARGET_DESTROYED land_soft_active=0`, `BDA destroyed → RTB not LAND_SOFT`, `BDA destroyed never LAND_SOFT` | **PASS** |
| BDA `MISS/REATTACK` | p1=4 exact string `MISS/REATTACK` → **ABORT** (cleared); WORK re-SEARCH; not FTS | `AP_UPLINK_MISS_REATTACK==4`; handle → ABORT; tests `MISS/REATTACK → ABORT`, `… then WORK → SEARCH re-cue`, `… stays ABORT not FTS`, `… does not assert FTS outputs` | **PASS** |
| No param1=5 / no bare MISS\|REATTACK wire names | No separate uplink; aliases map to 4; bare strings unknown | Header comment “no param1=5”; `AP_UPLINK_MISS`/`REATTACK` = 4; tests `bare MISS unknown`, `bare REATTACK unknown`; no `= 5u` uplink macro in ICD | **PASS** |
| LAND_SOFT submode | ABORT + `land_soft_active`; reason `LAND_SOFT`; not enum 7 | `mission_state.h` enum 0–6 only; `land_soft_active` flag; tests `miss → ABORT LAND_SOFT`, `land_soft_active=1`, `reason LAND_SOFT` | **PASS** |
| LAND_SOFT arming path | Only auto post-CLOSE `miss_hold_done` when `test_recover` latched at WORK→SEARCH | `try_enter_search` latches `test_recover_armed`; CLOSE miss path sets LAND_SOFT; tests `test_recover latched at SEARCH`, mid-CLOSE flip covered in `test_test_recover_land_soft` | **PASS** |
| LAND_SOFT vs TARGET_DESTROYED | Destroyed never enters LAND_SOFT even if recover armed | `test_bda_destroyed_not_land_soft` | **PASS** |
| Lost-link → RTB | `AP_LOST_LINK_RTB_S` / `T_link` = **3 s** no HEARTBEAT → RTB | Macro + `AP_SM_DEFAULT_T_LINK_S`; tests `AP_LOST_LINK_RTB_S == 3`, `link loss < 3s stays`, `lost-link → RTB` | **PASS** |
| Lost-link never auto-FTS | Lost-link does not assert FTS outputs; FTS only cmd / `hard_fail` | Lost-link branch → RTB only; tests `lost-link does not assert FTS outputs`; `hard_fail → FTS` separate | **PASS** |
| LAND_SOFT lost-link exception | During LAND_SOFT, auto lost-link does **not** → RTB | `land_soft_hold` guard in tick; tests `LAND_SOFT lost-link stays ABORT`, `land_soft still active after lost-link` | **PASS** |
| Explicit RTB vs LAND_SOFT | RTB cmd overrides LAND_SOFT | tests `explicit RTB overrides LAND_SOFT`, `land_soft cleared on RTB` | **PASS** |
| `operator_lock_request` | SEARCH→LOCK needs corridor + box; no map-only (no box) | SEARCH fail-HOLD if `!corridor_loaded`; lock only if `track_box_valid`; tests `operator_lock → LOCK`, `operator_lock ignored without box`, ignored in BOOT / non-SEARCH | **PASS** |
| Ground cmds WORK/ABORT/RTB/FTS | Exact strings accepted per state rules | `handle_command` strcmp on frozen names; tests WORK entry, ABORT/RTB/FTS paths, `WORK/ABORT ignored in RTB`, name table in `test_rtb_and_names` | **PASS** |
| Ground cmds BDA | `TARGET_DESTROYED`, `MISS/REATTACK` | Same as BDA rows above | **PASS** |
| Forbidden: terminal stick | CLOSE = commanded-miss hold; miss → ABORT (not impact) | Code comments + `miss_hold_done` → ABORT; unit tests cover miss→ABORT. **No guidance/setpoint unit test that proves absence of impact-seeking outside SM** | **PASS (SM)** / residual guidance integration |
| Forbidden: combat-intercept state | No INTERCEPT enum / state | `ap_mission_state_t` BOOT…FTS 0–6; name tests; no INTERCEPT symbol in SM | **PASS (structural)** |
| Forbidden: explosive FTS | Idle + surfaces fixed + fuel-cut only | `ap_mission_sm_fts_outputs` asserts those three only; test `FTS outputs idle+surfaces+fuel-cut`; no explosive API | **PASS (API)** |
| Forbidden: SEARCH without WORK+corridor | WORK + `corridor_loaded` + BIT required | `try_enter_search`; tests `boot stays without WORK`, `no SEARCH without corridor`, `WORK + corridor → SEARCH` | **PASS** |
| WORK ignored in LOCK/CLOSE | Per V1_PLAN §6 | tests `WORK ignored in LOCK`, `WORK ignored in CLOSE` | **PASS** |
| FTS latch | Latched until reset / power-cycle | tests `FTS stays latched`, `reset → BOOT (sim power cycle)` | **PASS** |

---

## Gaps / risks

| ID | Gap | Severity | Notes |
|----|-----|----------|-------|
| G-FTS | **FTS holder unnamed** | Process — **blocks G2/G3 range schedule** (`V1_PLAN` §7 / B8) | Not a unit-test failure; range go/no-go remains blocked until holder named + present when RF/arming live. |
| G-INT | Bridge string ↔ USER_1 param1 | Residual | SM tests use **string** cmds; ICD macros for p1=3/4 are compile-checked. Live Orin/GCS bridge mapping not exercised by `test_mission_sm`. |
| G-GUIDE | Terminal-stick absence outside SM | Doc / residual | SM forbids impact path via `miss_hold_done`→ABORT only; PX4/setpoint producers not in this test binary. |
| G-CORRIDOR | SEARCH corridor drop while already in SEARCH | Code present, thin test | `SEARCH fail-HOLD — no corridor` in `mission_sm.c`; no dedicated PASS line for mid-SEARCH corridor drop (entry path is tested). |
| G-HW | Non-explosive FTS hardware relay | Residual | Software asserts `fuel_cut_relay` flag only; iron-bird / G2 evidence of physical relay not in this audit. |
| G-LEGACY | Legacy C++ sketches | Low | `state_machine.cpp` / `*.hpp` marked non-deliverable in README; audit treats `mission_sm.[ch]` as SoT. |

---

## Verdict

### **GREEN** — V1 software SAFETY gates (unit)

- Rebuild + `./build/test_mission_sm` exit code **0**; all listed acceptance behaviors for BDA, LAND_SOFT, lost-link, operator lock, ground cmds, and forbidden SEARCH/FTS paths are covered by named tests and match `mission_sm.c` / frozen ICD.
- Mission profile remains N250 EO-only see/hold/commanded miss (not kill).

### Process caveat (does not flip unit verdict)

- **G2/G3 range schedule remains blocked** until an **FTS holder is named** (and present for live RF/arming per `V1_PLAN` §7). Track as process residual risk G-FTS, not as a failed SM unit test.

---

*Generated 2026-09-16 from on-box rebuild of `onboard/safety_gates` and source cross-check. English only.*
