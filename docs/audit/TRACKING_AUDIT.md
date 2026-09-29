# Tracking audit — Ground EO handoff + class-gated tracks

**Date:** 2026-09-16 (America/Chicago)  
**Owner:** Tracking system  
**Scope:** `/workspace/actprove-drone/tracking/` + `interfaces/include/actprove/`  
**Specs:** `docs/notes/GROUND_EO_CUE_AND_BDA.md`, `docs/icd/ground_eo_handoff.md`, `docs/icd/detection_msg.md`  
**Virtual test binary:** `tracking/build/tracking_virtual_tests`  
**Result:** **ALL PASS (0 failed)** — 23 assertions

---

## 1. Executive summary

| Area | Status | Notes |
|------|--------|-------|
| Ground EO seed (`seedFromGround` / `cam_id=2`) | PASS | Image box required; map-pin rejected |
| Nose handoff (same `class_id`) | PASS | → `HandoffPhase::Onboard` + `onboard_owns` |
| Class gate `{shahed_136, geran_2, gerbera}` | PASS | Legacy `quad=10` seed rejected |
| Class mismatch blocks handoff | PASS | Seed shahed / nose gerbera → no Onboard |
| BDA `MISS` re-seed without BOOT | PASS | `PostMiss` → new Ground seed OK |
| BDA `TARGET_DESTROYED` | PASS | Clears track; re-seed blocked until `reset()` |
| PWM / guidance | N/A (out of scope) | None in package |
| Thermal dual-stream | DEFER V1.1 | Wire keeps `CamId::kThermal=1` |

**Verdict:** V1 tracking handoff + class gate is fit for iron-bird / range-test integration with Perception (`cam_id=2`) and Safety (BDA → RTB).

---

## 2. Code under audit

| Path | Role |
|------|------|
| `tracking/include/tracking/tracker.hpp` | API: `update`, `seedFromGround`, `onBda` |
| `tracking/src/tracker.cpp` | Associate, coast, handoff, class gate |
| `interfaces/include/actprove/detection_msg.hpp` | `CamId`, `ClassId`, `isRangeTestClass` |
| `interfaces/include/actprove/ground_cue_msg.hpp` | GCS operator Lock cue |
| `interfaces/include/actprove/track_msg.hpp` | `HandoffPhase`, `onboard_owns`, lock fields |
| `tracking/src/virtual_tests.cpp` | Host virtual suite |
| `docs/icd/ground_eo_handoff.md` | ICD twin |

---

## 3. Class gate (range-test)

| Name | `class_id` | Lock / handoff |
|------|------------|----------------|
| `shahed_136` | 0 | Allowed |
| `geran_2` | 1 | Allowed |
| `gerbera` | 2 | Allowed |
| `quad` (legacy) | 10 | **Rejected** when `enforce_range_test_classes=true` (default) |
| `fixed_wing` (legacy) | 11 | **Rejected** |

Enforced in:

- `seedFromGround` — reject non-range-test classes  
- `pickPrimary` (cold start) — skip non-allowed boxes  
- `tryHandoffFromNose` — requires `classAllowed(seed_class_)` and nose box `class_id == seed_class_`

Manual designate conf from Perception is expected `1.0` (not asserted in unit tests).

---

## 4. Handoff state machine (audited)

```
Idle ──seedFromGround / cam_id=2──► GroundSeeded/AwaitNose
                                      │
                                      │ nose EO same class_id
                                      ▼
                                   Onboard (onboard_owns=true → CLOSE)
                                      │
                    ┌─────────────────┴─────────────────┐
                    │ onBda(MISS)                       │ onBda(TARGET_DESTROYED)
                    ▼                                   ▼
                 PostMiss                            Idle (reseed blocked)
                    │
                    └── seedFromGround ──► AwaitNose (no BOOT)
```

CLOSE eligibility = `onboard_owns == true` (nose ownership). Ground seed alone sets soft lock / `operator_lock_req` for Safety SEARCH→LOCK path but **does not** set `onboard_owns`.

---

## 5. Virtual test results

Command:

```bash
cmake -S tracking -B tracking/build && cmake --build tracking/build -j
./tracking/build/tracking_virtual_tests
```

| Test | Result |
|------|--------|
| geran happy path: seed → handoff → lock | PASS |
| class mismatch blocks handoff | PASS |
| legacy quad seed rejected | PASS |
| map-pin (w/h=0) rejected | PASS |
| BDA MISS → re-seed without BOOT | PASS |
| BDA destroyed → re-seed blocked; `reset()` clears | PASS |
| classes 0/1/2 each hand off | PASS |
| `DetectionMsg` `cam_id=2` seeds like `GroundCueMsg` | PASS |

Log excerpt: `=== ALL PASS (0 failed) ===`

---

## 6. Findings / residual risks

| ID | Severity | Finding | Action |
|----|----------|---------|--------|
| T1 | Low | IoU association still applies after Onboard; FOV jump after handoff uses class+conf only at handoff instant — good. Post-handoff large pixel jumps gated by `max_center_jump_px`. | Monitor on real FOV change |
| T2 | Medium | No IMM / bearing-only filter yet — `bearing_ned_rad` is pass-through from cue. Nav must treat as SEARCH bias until onboard box exists. | Nav ICD alignment |
| T3 | Low | `TARGET_DESTROYED` blocks re-seed until `Tracker::reset()` — Safety/WORK cycle must call reset on new engagement. | Document in Safety bring-up |
| T4 | Info | Thermal path unused (V1.1). | None |
| T5 | Info | Host has no JetPack; tests are logic-only (no camera). | G2 iron-bird next |

No blockers for continuing Perception/Safety integration.

---

## 7. Integration checklist (next)

- [ ] Perception publishes Ground Lock as `DetectionMsg` `cam_id=2`, conf=1.0, class ∈ {0,1,2}
- [ ] Comms/GCS uplink delivers cue or boxes to Orin Tracking consumer (SPSC / ZMQ)
- [ ] Safety: `operator_lock_request` + `onboard_owns` gating for CLOSE
- [ ] Safety: BDA marks call `onBda`; new WORK calls `reset()` after destroy
- [ ] Test card: no-radar ground-cue pass (TEST owns Gx)

---

## 8. Sign-off

**Tracking audit:** PASS (virtual)  
**Author:** Tracking system  
**Ping:** PM notified on completion
