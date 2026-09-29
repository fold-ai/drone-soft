# Range-test detect → Lock (class gate)

## Class gate (V1 range test)

Lock allowed only if `class_id ∈ {0, 1, 2}`:

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Legacy `quad` / `fixed_wing` are **not** in the gate. Operator may force a class on Manual Lock (still must be one of the three for this range test).

Config: `configs/classes_range_v1.yaml`, `configs/detector_yolov8n.yaml`.

## Train command

```bash
cd vision
python3 tools/dataset/ingest_raw.py --inbox data/inbox --out data/raw
python3 tools/dataset/label_qc.py --labels data/raw
python3 tools/dataset/split_sets.py --root data/raw --out data/manifests
# Dry-run (data.yaml only):
python3 tools/train/launch_train.py --dry-run
# Short CPU train (nice-to-have):
python3 tools/train/launch_train.py --epochs 5 --device cpu --batch 4
```

Requires: `pip install ultralytics` (and torch CPU if needed).

## Artifact paths

| Artifact | Path |
|----------|------|
| Class map | `configs/classes_range_v1.yaml` |
| YOLO data.yaml | `configs/data_range_v1.yaml` (generated) |
| Manifests | `data/manifests/{train,val,test}.txt` |
| Train runs | `runs/detect_range_v1/train/` |
| Best weights | `runs/detect_range_v1/train/weights/best.pt` |
| ONNX stub | `models/onnx/yolov8n_range_v1.onnx` (export optional) |
| TRT engine (deploy) | `models/engines/yolov8n_fp16.engine` |

## CamId (wire)

- Nose EO = `0`
- THERMAL = `1` (unused V1)
- Ground EO (GCS USB) = `2`


## Short train run (2026-09-16 CT)

- Env: `vision/.venv` + ultralytics 8.x, device=cpu, epochs=3, batch=4, imgsz=640
- Command: `.venv/bin/python tools/train/launch_train.py --epochs 3 --device cpu --batch 4 --name train_e3`
- Best weights: `runs/detect_range_v1/train_e3/weights/best.pt`
- ONNX: skipped (stub path `models/onnx/yolov8n_range_v1.onnx`)
- Note: labels are provisional full-frame boxes — replace with real boxes before field deploy.
