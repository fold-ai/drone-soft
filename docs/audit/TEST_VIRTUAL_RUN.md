# TEST virtual run — G1-style + G-RANGE-NR-V1

| Field | Value |
|-------|-------|
| Date | **2026-09-16** (America/Chicago) |
| Owner | **test** (pass/fail consolidator) |
| Scope | Software-only / simulated — **no** aircraft HW, **no** radar, **no** Jetson camera |
| Cards | G1-style bench criteria · **G-RANGE-NR-V1** (`test/g_range_no_radar/TEST_CARD.md`) |
| Specs | V1_PLAN §7 · `docs/notes/GROUND_EO_CUE_AND_BDA.md` · SAFETY V1 |
| Language | English |

## Executive go / no-go

| Path | Verdict | Why |
|------|---------|-----|
| **G1-style virtual software suite** | **GO** | Safety SM, nav, logger, tracking (+ virtual), G1 scorer PASS+FAIL fixtures, FTS-G1 all green |
| **G-RANGE-NR-V1 procedure readiness** | **CONDITIONAL GO** | Card + schemas present; Tracking/Safety BDA semantics green in SW; **no** live Ground EO / FTS holder / corridor ops on this box |
| **G2 iron-bird** | **NO-GO** | FTS holder **unnamed** (hard block); serial TELEM not present (`open=0`); Integration DRDY→HTE HOLD |
| **G3 flight** | **NO-GO** | Same FTS block + envelope not unlocked; no speed increase while G3 red |

**Bottom line:** Software path for see→lock→commanded-miss→operator BDA is **virtually green**. Hardware sync + named FTS holder still block any G2/G3 schedule. This run does **not** raise own-ship speed.

---

## Pass / fail table (this run)

| Channel | Command / artifact | Result | Notes |
|---------|-------------------|--------|-------|
| Safety SM | `onboard/safety_gates/build/test_mission_sm` | **PASS** | Exit 0; ALL TESTS PASSED (WORK-gated SEARCH, lost-link→RTB, lost-track→SEARCH→ABORT, FTS latch, BDA/LAND_SOFT) |
| Navigation | `navigation/build/navigation_smoke` | **PASS** | Exit 0; miss + soft-land + cue/BDA/RTB |
| Logger | `onboard/build/logger_smoke` | **PASS** | Exit 0; blackbox under `/tmp/actprove_blackbox/…` |
| MAVLink bridge | `onboard/build/mavlink_bridge_smoke` | **PASS*** | Exit 0 scaffold; `open=0` (no `/dev/ttyTHS1`). *Not a TELEM2 hardware pass |
| Tracking smoke | `tracking/build/tracking_smoke` | **PASS** | Seed / handoff / BDA miss / destroyed |
| Tracking virtual | `tracking/build/tracking_virtual_tests` | **PASS** | 23/23; map-pin reject; class gate; BDA |
| Ground EO stub | `vision/build/ground_eo_node` | **PASS (soft)** | Emits Ground EO `cam_id=2` frames without camera |
| G1 scorer (example) | `score_g1.py` on example archive | **PASS** | ≥60 Hz, p95 &lt;40 ms, PPS frames+IMU |
| G1 scorer (synthetic PASS) | `/tmp/g1_virtual_pass` | **PASS** | Fixture for CI-style check |
| G1 scorer (synthetic FAIL) | `/tmp/g1_virtual_fail` | **PASS (expected FAIL)** | Scorer correctly rejects bad Hz/latency/sync |
| FTS gate G1 | `check_fts_gate.py G1` | **PASS** | Holder not required |
| FTS gate G2 unnamed | `check_fts_gate.py G2` | **PASS (expected FAIL)** | Exit ≠0 — blocks schedule |
| FTS gate G2 named | `G2 --fts-holder Virtual` | **PASS** | Gate opens only when named |
| FTS gate G3 unnamed | `check_fts_gate.py G3` | **PASS (expected FAIL)** | Blocks schedule |
| FTS gate G3 named | `G3 --fts-holder Virtual` | **PASS** | |
| Layout checker vs example | `check_layout.py` example | **FAIL** | Example uses `manifest.json`; checker wants `meta.json` — **doc/tool mismatch** (non-blocking for G1 math) |
| Layout checker vs synthetic | `/tmp/g1_virtual_pass` | **PASS** | |
| G-RANGE card + schemas | `test/g_range_no_radar/` | **PASS** | `TEST_CARD.md` + schemas parse / required keys OK |
| G-RANGE scored archive | example JSON vs schemas | **SKIP** | No scored range archive yet; `jsonschema` not installed on box |
| Vision YOLO / train | GPU path | **SKIP** | HW/heavy; leftover `vision/runs/audit_smoke/summary.json` not re-run |
| Full SITL | `sim/` | **SKIP** | Placeholder only |

\* Scaffold PASS ≠ range TELEM proof.

---

## Peer channel stamps (coordinated)

Pulled from existing audits under `docs/audit/` (same day):

| Channel audit | Verdict | Relevance to this suite |
|---------------|---------|-------------------------|
| `SAFETY_AUDIT.md` | GREEN (SW) | Matches SM PASS; FTS holder still blocks G2/G3 |
| `NAVIGATION_AUDIT.md` | CONDITIONAL PASS | Smoke PASS; HW sync HOLD |
| `TRACKING_AUDIT.md` | PASS 23/23 | Ground EO handoff + BDA paths for G-RANGE |
| `COMMS_AUDIT.md` | PASS + OPEN | ICD OK; near-field video SKU open |
| `INTEGRATION_AUDIT.md` | buy FAIL / sync HOLD | DRDY→HTE blocks iron-bird timebase |
| `PM_ROLLUP.md` | Software largely PASS; HW FAIL/HOLD | Aligned with TEST consolidator |

---

## G-RANGE-NR-V1 mapping (virtual)

| Card step | Virtual evidence | Status |
|-----------|------------------|--------|
| Ground EO cue | Tracking `seedFromGround` + ground_eo_node stub | SW PASS |
| Manual Lock (image box) | Map-pin reject tests PASS | SW PASS |
| WORK → SEARCH | Safety SM WORK-only entry | SW PASS |
| CLOSE commanded miss | Nav smoke miss path | SW PASS |
| `TARGET_DESTROYED` → RTB | Safety + Tracking BDA | SW PASS |
| `MISS` / `REATTACK` → re-WORK | Tracking PostMiss re-seed; SM ABORT/re-WORK | SW PASS |
| Named FTS holder | Still **unnamed** (user skipped naming) | **BLOCK** for live range |
| No radar | Config / card rule | Assumed in SW; enforce on range day |

---

## Blockers (unchanged)

1. **FTS holder unnamed** — G2/G3 schedule blocked; G-RANGE live go blocked until named.  
2. **DRDY→HTE / PPS-IMU Integration HOLD** — real G1 on aircraft still waiting HW sync proof.  
3. **No TELEM2 device on this box** — MAVLink smoke is scaffold-only.  
4. **Layout tool vs example naming** (`meta.json` vs `manifest.json`) — fix under `test/g1_g3/` (TEST backlog).

---

## Fixtures used

- `/tmp/g1_virtual_pass/` — good G1 streams  
- `/tmp/g1_virtual_fail/` — deliberate fail for scorer negative test  

## Artifacts

- This file: `docs/audit/TEST_VIRTUAL_RUN.md`  
- Cards: `test/g1_g3/`, `test/g_range_no_radar/TEST_CARD.md`

---

## Sign-off

| Role | Stamp |
|------|-------|
| TEST | Virtual suite recorded **2026-09-16** — G1-SW **GO**; G-RANGE procedure **CONDITIONAL GO**; G2/G3 **NO-GO** until FTS holder + HW sync |
