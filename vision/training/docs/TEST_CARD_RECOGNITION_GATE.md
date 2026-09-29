# Test card — Recognition training gate (V1)

**ID:** REC-TRAIN-GATE-V1  
**Owner:** test (pass/fail) · Perception runs train/eval · Tracking consumes class gate  
**Language:** English  
**Scope:** EO detector for class-gated Lock on `{shahed_136, geran_2, gerbera}` only. **No radar. No thermal (V1).**  
**Parents:** `vision/training/README.md`, `vision/DATASET.md`, `vision/notes/RANGE_TEST_DETECT.md`

## Purpose

Gate a trained weight set before it may drive **auto Lock** on the range stack. Operator Manual Lock (forced class ∈ {0,1,2}) is out of scope for this card’s auto-detect criteria but still must not accept map-pin-only Lock.

## Classes under test

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Any other `class_id` in Lock path = **FAIL** this gate (even if mAP looks good).

## Preconditions

| # | Check |
|---|--------|
| 1 | Eval split from sequence-safe manifests (`data/manifests/test.txt` or documented hold-out) — **no train-clip leakage** |
| 2 | Labels are real boxes (not provisional full-frame) **or** run is explicitly marked `labels=provisional` → then result is **CONDITIONAL** at best, never field GO |
| 3 | Negatives set present for FP test (≥10% of eval images preferred; if missing, FP-on-negatives = **FAIL / BLOCK** until provided) |
| 4 | Same imgsz / conf / IoU as deploy config (`configs/detector_yolov8n.yaml` or stamped export) |
| 5 | Metrics JSON archived under `vision/training/runs/<run_id>/` (and optional Supabase upload per `TRAINING_METRICS_SUPABASE.md`) |

## Pass thresholds (all required)

Measured on the **held-out test** split unless noted. Conf / NMS = deploy defaults.

### A) mAP

| Metric | Threshold | Fail if |
|--------|-----------|---------|
| mAP@0.50 (all 3 classes) | ≥ **0.60** | below |
| mAP@0.50:0.95 (all 3) | ≥ **0.35** | below |
| Per-class AP@0.50 (`shahed_136`, `geran_2`, `gerbera`) | each ≥ **0.50** | any class below |

### B) Class confusion (FAIL conditions)

Build 3×3 confusion on **matched** detections (IoU ≥ 0.50) among the three gated classes.

| Rule | Threshold | Fail if |
|------|-----------|---------|
| Pairwise confusion rate | For every ordered pair \(i≠j\): `count(pred=j \| gt=i) / count(gt=i)` | **> 0.15** |
| Dominant wrong class | Any off-diagonal cell | **> 25%** of that GT class’s matched preds |
| Ungated class emission | Predictions with `class_id ∉ {0,1,2}` on eval | **any** at deploy conf → **FAIL** |

Symmetric note: confusing `geran_2` ↔ `gerbera` still fails the pairwise rule — do not waive.

### C) False positives on negatives

Eval on images labeled **negative** (empty sky/ground — no target box).

| Metric | Threshold | Fail if |
|--------|-----------|---------|
| Image-level FP rate | Share of negative images with ≥1 detection at deploy conf | **> 0.05** (5%) |
| Mean dets / negative image | — | **> 0.10** |

If negatives folder empty / not in manifest → **BLOCK** (cannot PASS this card).

## Scoring procedure

```bash
cd vision
# Produce metrics JSON (example — use project eval script when present)
python3 tools/train/eval_gate.py \
  --weights runs/detect_range_v1/<run>/weights/best.pt \
  --data configs/data_range_v1.yaml \
  --split test \
  --negatives data/raw/negatives_or_manifest \
  --out training/runs/<run_id>/gate_metrics.json
```

If `eval_gate.py` is not landed yet: Ultralytics `val` for mAP + confusion, plus a negatives-only pass that counts detections at deploy conf. TEST still owns pass/fail against this card.

## Evidence pack (archive required)

```
REC_YYYYMMDD_HHMMSS_<run_id>/
  manifest.json          # weights path, git sha, imgsz, conf, IoU, label quality flag
  gate_metrics.json      # map50, map50_95, per_class AP, confusion 3x3, fp_on_negatives
  confusion.png          # optional plot
  PASS | FAIL | CONDITIONAL   # marker file written by TEST only
  notes.md               # optional
```

### `gate_metrics.json` required keys

`map50`, `map50_95`, `ap50_per_class` (object with three class keys), `confusion_3x3`, `confusion_rate_ij`, `fp_image_rate_negatives`, `mean_dets_per_negative`, `ungated_class_count`, `weights_path`, `split`, `label_quality` (`real`|`provisional`).

## Results

| Result | Meaning |
|--------|---------|
| **PASS** | All of A–C met; labels=`real`; negatives evaluated |
| **CONDITIONAL** | Metrics meet A–C but labels=`provisional` **or** agreed waiver logged by TEST — **not** field Lock GO |
| **FAIL** | Any A–C miss, ungated class emission, or map-pin Lock path used in eval harness |
| **BLOCK** | Missing negatives / missing test split / no metrics JSON |

**Field rule:** Only **PASS** unlocks auto Lock on `{0,1,2}` for range software. CONDITIONAL stays train/bench only.

## Relationship to other gates

| Gate | Relationship |
|------|----------------|
| G1 (Hz / latency / PPS) | Orthogonal — recognition PASS does not green G1 |
| G-RANGE-NR-V1 | Needs usable detect/Lock; this card gates the **weights** |
| Tracking class gate | Must remain `{0,1,2}` regardless of mAP |

## Non-goals

- Radar / thermal metrics  
- Kill / hit scoring  
- Accepting full-frame provisional labels as field GO  
- Waiving geran↔gerbera confusion  

## Sign-off

| Role | Action |
|------|--------|
| Perception | Deliver weights + `gate_metrics.json` |
| TEST | Score vs this card; write PASS/FAIL/CONDITIONAL/BLOCK |
| PM | Schedule only on PASS (or explicit CONDITIONAL bench use) |
