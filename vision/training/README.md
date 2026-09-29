# Target recognition training system (actprove-drone)

**Owner channels:** perception (lead), Tracking, test, System stack (metrics export), hardware (Orin train notes).

## Goal

Train / evaluate **class-gated** detectors for Lock on:

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Replace provisional **full-frame** labels with real boxes. Produce deployable weights for Orin (TensorRT later).

**Layout:** isolated under `vision/training/` (`datasets/`, `runs/`, `exports/`). Inbox remains `vision/data/inbox/`.

## Exact commands (from `vision/`)

Use `vision/.venv`.

### 1. Ingest inbox → training datasets

```bash
.venv/bin/python training/scripts/ingest_inbox.py
# → training/datasets/raw/{images,labels,meta.jsonl}
# provisional full-frame boxes marked provisional=true
```

### 2. Label (real boxes)

See [`docs/LABELING.md`](docs/LABELING.md).

```bash
# Empty sidecars for labeling tools:
.venv/bin/python training/scripts/init_labels.py --empty

# After Label Studio / CVAT / YOLO export:
.venv/bin/python training/scripts/import_yolo_labels.py --from-dir /path/to/labels/
# or
.venv/bin/python training/scripts/import_labelstudio.py --yolo-dir /path/to/export/
```

### 3. QC (rejects full-frame by default)

```bash
# Production — MUST pass without --allow-provisional:
.venv/bin/python training/scripts/label_qc.py

# Smoke ONLY (provisional OK):
.venv/bin/python training/scripts/label_qc.py --allow-provisional
```

Full-frame junk = area≥0.95 or w,h≥0.98 → **REJECT** unless `--allow-provisional`.  
**Production gate FAILS without real boxes.**

### 4. Split manifests

```bash
.venv/bin/python training/scripts/split_manifests.py --smoke-per-class 30
# full → training/datasets/manifests/{train,val,test}.txt
# smoke subset → training/datasets/smoke/{train,val,test}.txt
```

### 5. Train

```bash
# Production-ish (after real labels + QC pass):
.venv/bin/python training/scripts/train.py \
  --epochs 30 --device cpu --batch 8 --imgsz 640 --name run1

# Smoke (provisional + subset):
.venv/bin/python training/scripts/train.py \
  --smoke --allow-provisional \
  --epochs 2 --device cpu --batch 4 --imgsz 640 --name smoke_e2
```

Weights → `training/runs/<name>/weights/best.pt`

### 6. Eval

```bash
.venv/bin/python training/scripts/eval.py \
  --weights training/runs/smoke_e2/weights/best.pt \
  --name smoke_e2 --device cpu --batch 4 --smoke
```

Reports mAP50, mAP50-95, per-class; **FP on negatives** (N/A if count=0).

### 7. Export ONNX

```bash
.venv/bin/python training/scripts/export_onnx.py \
  --weights training/runs/smoke_e2/weights/best.pt \
  --name smoke_e2 --stub-ok
# → training/exports/smoke_e2.onnx (or .STUB.txt if export fails)
```

Orin TRT: [`docs/ORIN_TRT.md`](docs/ORIN_TRT.md) (build on device later).

### 8. Metrics JSON

```bash
.venv/bin/python training/scripts/write_metrics_json.py \
  --run-dir training/runs/smoke_e2 \
  --model yolov8n --epochs 2 --split val \
  --notes "smoke provisional labels"
# → training/runs/smoke_e2/metrics.json
```

Schema: [`docs/TRAINING_METRICS_SUPABASE.md`](docs/TRAINING_METRICS_SUPABASE.md). Local only; upload no-ops without keys.

## Docs

| Doc | Topic |
|-----|-------|
| [`docs/PIPELINE.md`](docs/PIPELINE.md) | One-pager |
| [`docs/LABELING.md`](docs/LABELING.md) | Label Studio / CVAT / YOLO; reject full-frame |
| [`docs/NEGATIVES.md`](docs/NEGATIVES.md) | Hard negatives → `data/inbox/negative/` |
| [`docs/ORIN_TRT.md`](docs/ORIN_TRT.md) | On-device TRT |
| [`docs/SAFETY_CLASS_GATE.md`](docs/SAFETY_CLASS_GATE.md) | Lock only `{0,1,2}` |
| [`docs/TEST_CARD_RECOGNITION_GATE.md`](docs/TEST_CARD_RECOGNITION_GATE.md) | Pass/fail gate |

## Negatives

Currently empty. Collect sky / birds / civilian aircraft / ground clutter into `vision/data/inbox/negative/` — see [`docs/NEGATIVES.md`](docs/NEGATIVES.md).

Passive hard-negative readiness / FP audit (separate from the class-gated
recognition gate):

```bash
../.venv-nose-eo/bin/python training/scripts/audit_hard_negatives.py \
  --weights yolov8n.pt
```

## Cloud (later)

User provisions **separate** Vercel + Supabase. Never commit keys.

## Boundary vs actprove-ops

Ops sim = operator trainer. **This** tree = real ML for recognition. Do not touch actprove-ops from this workstream.
