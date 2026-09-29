# Perception Audit Report — EO Range V1

- **Date:** 2026-09-16
- **Owner:** Perception
- **Scope:** EO-only perception and Ground EO USB dry-run; thermal excluded

## Summary

| Area | Result | Evidence / qualification |
|---|---|---|
| Range V1 class map | PASS | `shahed_136=0`, `geran_2=1`, `gerbera=2` agrees across config, wire interface, metadata, labels, and detector YAML. |
| Virtual validation smoke | PASS (execution) | 12 validation images processed with `best.pt`; 18 detections emitted to JSONL. This is not a quality or accuracy sign-off. |
| Ground EO USB dry-run | PASS (scaffold) | Config, API, node, notes, and compiled target are present. `/dev/video0` is absent; the V4L2 scaffold's soft-open path produced five synthetic manual-designate frames. |
| EO-only camera routing | PASS | Nose EO wire `cam_id=0`; thermal wire `cam_id=1` is reserved/unused; Ground EO wire `cam_id=2`. |
| Production retrain readiness | FAIL / blocked | All labels are provisional full-frame boxes, there are no negatives, and the available training artifact used only three CPU epochs. |

**Overall disposition:** Conditional scaffold pass only. Do not treat this audit as production detection validation.

## Class Map Validation

The required range-v1 map is consistent in `configs/classes_range_v1.yaml`, `iface/include/iface/box_message.hpp`, `data/inbox/MANIFEST.md`, `configs/data_range_v1.yaml`, and `configs/detector_yolov8n.yaml`.

| Class | Required ID | Interface ID | Inbox files | `meta.jsonl` | Label files / boxes | Train / val / test |
|---|---:|---:|---:|---:|---:|---:|
| `shahed_136` | 0 | 0 | 214 | 214 | 214 / 214 | 152 / 26 / 36 |
| `geran_2` | 1 | 1 | 251 | 251 | 251 / 251 | 175 / 39 / 37 |
| `gerbera` | 2 | 2 | 252 | 252 | 252 / 252 | 176 / 38 / 38 |
| **Total** |  |  | **717** | **717** | **717 / 717** | **503 / 103 / 111** |

Additional checks:

- `data/inbox/MANIFEST.md` reports 214, 251, and 252 files, matching the filesystem and metadata counts.
- `data/raw/meta.jsonl` has 717 records; all records use `cam_id=0` and `provisional_box=true`.
- All 717 label files contain one label and all 717 boxes are normalized full-frame boxes (`0.5 0.5 1.0 1.0`).
- `nc: 3`, names, and class ordering match in the detector dataset YAML and detector runtime YAML.
- The negative inbox is empty (0 files).

## Detect Smoke Results

The smoke used the existing virtual environment and weights; no model was trained.

- **Environment:** `/workspace/actprove-drone/vision/.venv`
- **Weights:** `vision/runs/detect_range_v1/train_e3/weights/best.pt`
- **Source list:** `vision/data/manifests/val.txt`, paths resolved relative to `vision/data/raw`
- **Sample:** first 12 validation images (of 103)
- **Processed:** 12 images
- **Detections:** 18 total; 17 with class ID 0, 1 with class ID 2, and 0 with class ID 1
- **Result:** smoke execution passed; the detections do not establish precision, recall, localization, or readiness

Output artifacts:

- `vision/runs/audit_smoke/predictions.jsonl` — one JSON object per detection with `file`, `class_id`, `conf`, and `box`
- `vision/runs/audit_smoke/summary.json` — run summary and provenance

The prevalence of near/full-frame predictions is consistent with the provisional full-frame labeling limitation. Those labels inflate the training signal and make this smoke unsuitable for claiming real object localization.

## Ground EO USB Dry-Run

Configuration and implementation checks passed:

- `configs/capture_ground_eo_usb.yaml`: `/dev/video0`, 30 fps, 1920x1080, `cam_id: 2`, V4L2, EO, GCS host.
- `capture/include/capture/ground_eo_capture.hpp` and `capture/src/ground_eo_capture.cpp`: Ground EO wrapper sets wire `cam_id=2` and `source_role=ground_eo`.
- `apps/ground_eo_node/main.cpp`: manual designate and detector paths emit `DetectionMsg` with `CamId::GroundEo`; class gate is limited to IDs 0, 1, and 2.
- `notes/GROUND_EO_MANUAL_LOCK.md` and `notes/GROUND_LOCK_HANDOFF.md`: image-box handoff and no-map-pin-lock rules are documented.
- Compiled target `build/ground_eo_node` and capture library are present.

`/dev/video0` does not exist on this host, so no live camera capture was attempted. The compile-level/open-stub dry-run used `build/ground_eo_node --manual`; it completed five synthetic frames and printed `cam_id=2 (GroundEo=2)` for each. The underlying V4L2 scaffold intentionally soft-opens a missing device, so this result is not hardware validation.

The ingest is Perception-owned. GCS exposes a **Nose EO | Ground EO** source toggle and emits cue/BDA; it does not create a map-pin lock. Ground EO manual lock is an image box and publishes `DetectionMsg` with `cam_id=2`.

## EO-Only Camera Routing

The locked wire mapping is:

| Stream | `CamId` | V1 status |
|---|---:|---|
| Nose EO | 0 | Active EO path |
| Thermal | 1 | Reserved and unused in V1 |
| Ground EO | 2 | GCS USB/V4L2 range-test path |

The detector runtime configuration uses `cam_id: 0` and the interface header marks thermal as reserved until V1.1. No thermal stream is part of this V1 runtime path; thermal was not exercised by this audit.

## Risks / Blockers

1. **Provisional full-frame labels:** all 717 labels are full-frame boxes rather than object boxes. This inflates the training signal and invalidates localization-quality conclusions.
2. **No negatives:** the negative set is empty, so false-positive behavior is not characterized.
3. **Short training artifact:** `train_e3` was a three-epoch CPU training run; it is a smoke artifact, not a serious model baseline.
4. **Live Ground EO unverified:** `/dev/video0` is absent on this host. Configuration, API wiring, compiled code, and stub behavior passed, but USB/V4L2 hardware, timing, and real frames remain untested.

## Artifact Paths

- `docs/audit/PERCEPTION_AUDIT.md`
- `vision/configs/classes_range_v1.yaml`
- `vision/configs/data_range_v1.yaml`
- `vision/configs/detector_yolov8n.yaml`
- `vision/iface/include/iface/box_message.hpp`
- `vision/data/inbox/MANIFEST.md`
- `vision/data/raw/meta.jsonl`
- `vision/data/manifests/train.txt`
- `vision/data/manifests/val.txt`
- `vision/data/manifests/test.txt`
- `vision/runs/detect_range_v1/train_e3/weights/best.pt`
- `vision/runs/detect_range_v1/train_e3/args.yaml`
- `vision/runs/audit_smoke/predictions.jsonl`
- `vision/runs/audit_smoke/summary.json`
- `vision/configs/capture_ground_eo_usb.yaml`
- `vision/capture/include/capture/ground_eo_capture.hpp`
- `vision/capture/src/ground_eo_capture.cpp`
- `vision/apps/ground_eo_node/main.cpp`
- `vision/notes/GROUND_EO_MANUAL_LOCK.md`
- `vision/notes/GROUND_LOCK_HANDOFF.md`
- `vision/build/ground_eo_node`

## Recommendation

Acquire and review real object bounding boxes, add hard-negative images, and establish a properly trained/evaluated baseline before any serious retrain or flight-relevant perception claim. Preserve the current three-class map and EO camera IDs while replacing provisional labels.
