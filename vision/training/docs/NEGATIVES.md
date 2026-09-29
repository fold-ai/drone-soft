# Hard negatives — false-positive control

Negatives are images with **no target** of the three gated classes. Labels are **empty** `.txt` files.

## Why

Eval reports **FP rate on negatives**. Without a negative set, `fp_on_negatives` is **N/A (count=0)** and the recognition gate treats FP-on-negatives as **BLOCK** until provided (`TEST_CARD_RECOGNITION_GATE.md`).

## Inbox path

```
vision/data/inbox/negative/
```

Drop photos here, then re-run ingest. Preferred share of eval set: **≥10%** negatives.

## Capture from the local GCS

With the Nose EO camera and local sidecar running, press **Mark false cue**
whenever the detector reacts to a tree, curtain, reflection, empty region, or
other non-target. The UI submits one JPEG only after the button press to:

```text
POST http://127.0.0.1:8765/dataset/negative/frame
```

The sidecar stores a deduplicated image in `vision/data/inbox/negative/` and
appends local capture context to `capture_meta.jsonl`. It does not continuously
record video and it does not change detection, tracking, or mission state.

## Readiness and passive FP audit

Run from the repository root:

```bash
.venv-nose-eo/bin/python vision/training/scripts/audit_hard_negatives.py \
  --weights vision/yolov8n.pt
```

The command writes `vision/training/exports/hard_negative_audit.json`. Until at
least 50 valid samples from at least 5 explicit capture sessions exist, its
status remains `BLOCKED`. Use the single-frame action for isolated errors or
the 10-frame action for a five-second moving scene; select the cause before
capture. With samples and weights present the audit reports image-level
detector emissions at confidence 0.25, 0.35, and 0.50. This is an offline demo
audit and does not enable tracking or flight control.

## What to collect

| Category | Examples |
|----------|----------|
| Sky / empty EO | Clear sky, clouds, sun glare, haze |
| Birds | Birds in flight (common Lock false cue) |
| Civilian aircraft | Airliners, light planes, helicopters at distance |
| Ground clutter | Trees, poles, towers, vehicles, buildings, roads |
| Sensor junk | Lens flare, compression blocks, motion blur empty frames |

**Do not** put gated-target photos in `negative/`.

## After drop

```bash
cd vision
.venv/bin/python training/scripts/ingest_inbox.py
.venv/bin/python training/scripts/split_manifests.py
.venv/bin/python training/scripts/label_qc.py   # negatives = empty labels OK
```

## Eval

`scripts/eval.py` scans `meta.jsonl` for `class=negative` and runs predict @ deploy conf.  
Reports `fp_on_negatives` = fraction of negative images with ≥1 detection.
