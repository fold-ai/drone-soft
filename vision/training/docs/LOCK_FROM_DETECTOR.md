# Lock acceptance from detector scores

**Owner:** Tracking (+ Perception training)  
**Scope:** When may an **auto** detector box become / sustain SEARCH→LOCK?  
**English.** Does not replace Safety WORK/corridor gates.

## Exports

| Artifact | Path |
|----------|------|
| Class map (JSON) | `vision/training/exports/class_map_range_v1.json` |
| Acceptance config | `vision/training/configs/lock_acceptance_v1.yaml` |
| Source YAML names | `vision/configs/classes_range_v1.yaml` |
| C++ mirror | `interfaces/include/actprove/class_map.hpp` |

Tracking **consumes** the exported class map + acceptance thresholds (see `tracking/` + `LockAcceptance`).

---

## 1. Class gate

Allowed Lock classes (range-test):

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Legacy `quad=10` / `fixed_wing=11` are **training-only / not Lock-eligible** unless operator forces a gated id.

---

## 2. Score and geometry gates (auto Lock)

A detection box is **accept-eligible** only if **all** hold:

| Check | V1 default | Rationale |
|-------|------------|-----------|
| `class_id ∈ gate` | `{0,1,2}` | Class-gated Lock |
| `conf ≥ min_conf` | **0.45** | Above raw YOLO publish floor (0.25); Lock is stricter |
| `w * h ≥ min_box_area_px` | **64** | Kill 1–2 px noise |
| `min(w,h) ≥ min_box_side_px` | **6** | Skinny spike rejection |
| ` (w*h)/(src_w*src_h) ≤ max_box_area_frac` | **0.85** | Reject provisional full-frame labels |

Detector may still **publish** lower-conf boxes for logging; Tracking must not promote them to Lock seed / confirm.

**Manual / Ground EO Lock:** operator box with `conf=1.0`, image box required, class still in gate (operator picks type).

---

## 3. Tracker confirmation

After a box is accept-eligible:

1. Associate across frames (`confirm_hits` default **3**).
2. `lock_quality ≥ lock_quality_min` (default **0.6**) → `LockState::LOCK`.
3. Ground→nose handoff still requires **same `class_id`** and nose `conf ≥ handoff_conf_min` (0.30), and nose box should pass accept gates when auto.

CLOSE remains gated by `onboard_owns` (nose ownership) per handoff ICD.

---

## 4. Relationship to YOLO config

`vision/configs/detector_yolov8n.yaml` uses `conf_thresh: 0.25` for NMS publish.  
**Lock `min_conf` (0.45) is intentionally higher** — do not lower Lock to match publish floor without a test-card change.

---

## 5. Wiring checklist

- [x] Export `class_map_range_v1.json`
- [x] Document acceptance thresholds
- [x] `actprove::class_map` + `LockAcceptance` headers
- [x] Tracker filters with min_conf / min area / gate from acceptance defaults
- [ ] Perception loads same JSON at runtime on Orin (follow-up)
- [ ] Test card: FP on negatives must not Lock

## 6. Regenerate export

```bash
python3 vision/training/scripts/export_class_map.py
```
