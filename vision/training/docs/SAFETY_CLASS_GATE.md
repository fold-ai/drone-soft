# SAFETY — class gate before Lock (training ≠ kill)

| Field | Value |
|-------|-------|
| Owner | SAFETY (+ perception implements gate) |
| Date | 2026-09-16 |
| Scope | `vision/training` → detect → Lock handoff |
| Mission | V1 see / hold / **commanded miss** — **not kill** |

Cross-refs: `onboard/safety_gates/` · `V1_PLAN.md` §6 · `vision/notes/RANGE_TEST_DETECT.md` · `vision/training/README.md`

---

## Rule

**Auto Lock is allowed only after a class-gated detection** on an allowed target class.  
Training better detectors does **not** authorize terminal stick, impact-seeking, or autonomous kill.

| Path | Requirement before LOCK |
|------|-------------------------|
| **AUTO_LOCK** | `track_box_valid` **and** `class_id ∈ {0,1,2}` (`shahed_136`, `geran_2`, `gerbera`) |
| **SEMI** | Detector proposes gated class; operator confirms |
| **MANUAL_LOCK** | Image box + `operator_lock_request`; corridor + WORK already in SEARCH; range-test class still one of the three if operator forces type |
| **Map / tower cue alone** | **Never** Lock |

SM still requires: **WORK** + **corridor_loaded** → SEARCH before any Lock path (`safety_gates`).

---

## Training must not enable kill

Allowed outcomes of training work:
- Higher recall/precision on the three classes
- Fewer false Locks on negatives / clutter
- Deployable weights for Orin (ONNX/TRT later)

**Forbidden** to introduce via training configs, labels, or export metadata:
- Kill / hit / impact / warhead / “terminal” objectives or loss terms
- Auto-BDA or auto-`TARGET_DESTROYED` from class score
- Bypass of WORK, corridor, or operator BDA (`MISS/REATTACK`, `TARGET_DESTROYED`)
- Treating class confidence as engagement complete

V1 CLOSE remains **commanded miss distance**. Operator BDA decides destroyed vs re-cue — not the classifier.

---

## Class map (range V1)

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Legacy `quad` / `fixed_wing` (and any other id) → **no auto Lock**.

Config source: `vision/configs/classes_range_v1.yaml`.

---

## Acceptance (TEST)

- Detection on disallowed class → no auto Lock.
- Training run / new weights merge → no change to FTS, BDA, or miss doctrine.
- Claiming a hit profile from class gate = **FAIL** (G3 miss 15–30 m doctrine).
