# Orin NX — YOLO train / TensorRT infer notes

**Owner:** hardware (this doc) · perception (train scripts) · Tracking (runtime budgets)  
**Module:** NVIDIA Jetson Orin NX **16GB** (program BUY)  
**Stamp:** 2026-09-16 CT  
**Related:** `vision/configs/trt_build.yaml` · `vision/tools/train/launch_train.py` · `vision/README.md` · BOM island 15–30 W sust / 60–80 W peak

---

## 1. Split of labor (do not confuse)

| Phase | Where | Why |
|-------|--------|-----|
| **Train** (Ultralytics YOLO) | **x86/ARM workstation GPU** preferred; Orin only for short smoke / fine-tune | Training wants large batch + long epochs; Orin island power/thermal is flight-sized |
| **Export** ONNX | Same machine that trained, or Orin | ONNX is portable |
| **Build TensorRT engine** | **On the target Orin** (JetPack / TRT version locked) | Engines are **not** portable across GPU arch / TRT versions |
| **Infer 60 Hz path** | Orin C++ / CUDA / TensorRT (`vision/detect`) | Python stays in `tools/` only |

V1 classes: `shahed_136` (0), `geran_2` (1), `gerbera` (2). EO-only G1.

---

## 2. Recommended path: YOLO → ONNX → TensorRT

### 2.1 Train (host)

```bash
cd vision
# After ingest + real boxes (not full-frame junk):
python3 tools/train/launch_train.py --epochs 30 --device cuda   # or cpu smoke
# Outputs under vision/runs/detect_range_v1/
```

Defaults from launcher: **imgsz=640**, YOLOv8n class, project `runs/detect_range_v1`.

### 2.2 Export ONNX (host or Orin)

```bash
# Example Ultralytics export (adjust weights path after train):
yolo export model=runs/detect_range_v1/train_*/weights/best.pt \
  format=onnx imgsz=640 opset=12 simplify=True
# Program stub path (docs): models/onnx/yolov8n_range_v1.onnx
mkdir -p models/onnx
cp runs/detect_range_v1/train_*/weights/best.onnx models/onnx/yolov8n_range_v1.onnx
```

Fixed input shape for deploy: **`[1, 3, 640, 640]`** (matches `trt_build.yaml`).

### 2.3 Build engine **on Orin**

Align with `configs/trt_build.yaml`:

| Key | Value | Note |
|-----|--------|------|
| precision | **fp16** first | Then INT8 only if calibrated and gated |
| batch | **1** | Flight path |
| workspace_mb | **1024** | Raise only if builder OOMs |
| dla | **false** | Default off |
| fixed_shape | `[1, 3, 640, 640]` | No dynamic shape in V1 hot path |

```bash
# On Orin (JetPack with TensorRT). Example trtexec:
/usr/src/tensorrt/bin/trtexec \
  --onnx=models/onnx/yolov8n_range_v1.onnx \
  --saveEngine=models/engines/yolov8n_fp16.engine \
  --fp16 \
  --memPoolSize=workspace:1024M \
  --shapes=images:1x3x640x640
```

Deploy path (program docs): `models/engines/yolov8n_fp16.engine`.  
Runtime loader: C++ `vision/detect/src/trt_engine.cpp`.

**Rule:** Rebuild engine after any JetPack / TRT / CUDA upgrade. Check engine into release artifacts with JetPack version tagged; do not assume laptop-built `.engine` runs on Orin.

---

## 3. Power (Orin NX island)

From BOM / hardware audit (planning envelopes, not a lab measurement):

| Mode | Expectation |
|------|-------------|
| Idle / desktop bring-up | Well below island sustained |
| Detect infer FP16 batch=1 @ 640 | Fits inside **15–30 W sustained** island budget with headroom for cam + IMU |
| TRT **engine build** | Short **high** GPU/CPU draw — treat like peak; use active cooling; avoid building while airborne |
| Train on Orin (discouraged) | Can push toward **peak 60–80 W** island class + throttle/thermal — prefer host GPU |

**Ops rules**

1. Active cooling required; log SoC / GPU temp during build and soak infer.  
2. Do not run multi-hour YOLO train on the flight island without a lab PSU and airflow plan.  
3. Galvanic-isolated cam rails ≠ ESC/ECU; infer load must not brown-out 12 V EO rail (Basler ≥5 W class headroom).

---

## 4. Disk layout (Orin NVMe)

Program: NVMe black-box **≥1–2 h** flight log — keep ML artifacts on a **separate partition or prefix** so logging cannot fill the rootfs.

Suggested layout on Orin:

```
/data/nvme/
  blackbox/          # flight logs (highest priority free-space reserve)
  models/
    onnx/
    engines/         # *.engine tagged by jetpack+trt+git_sha
  datasets/          # optional; prefer train offboard
  runs/              # only if fine-tuning on device
```

| Artifact | Size order (typical) | Keep on Orin? |
|----------|----------------------|---------------|
| `best.pt` | tens–hundreds MB | Optional (debug) |
| ONNX 640 YOLO-n | tens–low hundreds MB | Yes (rebuild source) |
| FP16 engine | tens–low hundreds MB | **Yes (deploy)** |
| Full raw dataset | GBs+ | **No** — train offboard |
| INT8 calibration cache | small | Yes if using INT8 |

**Disk hygiene:** `runs/` and inbox copies grow fast — rsync off, then delete. Never let training fill the black-box volume.

---

## 5. Docker / JetPack notes

### 5.1 Prefer NVIDIA L4T / JetPack base on Orin

- Use the JetPack version pinned by the carrier bring-up (Forecr DSBOARD-ORNX baseline when PO’d).  
- TensorRT, CUDA, and cuDNN must match the image that built the engine.  
- Official NGC **l4t-tensorrt** / **l4t-ml** style images are the default path; pin digest tags in a compose file when added.

### 5.2 What to containerize

| Work | Container? |
|------|------------|
| `trtexec` / engine build | Yes (L4T+TRT) or bare-metal JetPack |
| C++ detect node | Prefer bare-metal or privileged container with `--runtime nvidia` |
| Ultralytics train | Host workstation Docker/conda; Orin container only for smoke |
| Dataset ingest | Either; CPU OK |

### 5.3 Compose sketch (Orin lab)

```yaml
# illustrative — pin real image tags at bring-up
services:
  trt_build:
    image: nvcr.io/nvidia/l4t-tensorrt:<JETPACK_TAG>
    runtime: nvidia
    network_mode: host
    volumes:
      - ./models:/models
    working_dir: /models
    # command: trtexec ...
```

**Gotchas**

- `--runtime nvidia` required for GPU.  
- Mount model dirs RW for engine output.  
- UIDs: write engines as the service user that the detect node reads.  
- Do not bake secrets (Supabase keys) into images — metrics upload uses secure env (see `TRAINING_METRICS_SUPABASE.md`).

### 5.4 Host (x86) train Docker

Standard CUDA Ultralytics image on a desktop GPU is fine. Export ONNX there; **copy ONNX to Orin** for `trtexec`.

---

## 6. Latency / quality gates (infer)

| Gate | Target |
|------|--------|
| Sustained detect rate | ≥60 Hz class (G1) |
| Frame→box p95 | <40 ms |
| Timebase | PPS on every box / `measurement_epoch` |
| Soft timestamps | REJECT |

After each new engine: run soak on recorded EO + live Basler when carrier PASS; record GPU temp, power if available, and p95 latency.

---

## 7. Checklist — first Orin bring-up

- [ ] JetPack / TRT version recorded in `models/engines/README` (create when first engine lands).  
- [ ] Active cooling + temp logging.  
- [ ] NVMe mounted with blackbox vs models split.  
- [ ] ONNX from gated train (real boxes, class gate per `SAFETY_CLASS_GATE.md`).  
- [ ] `trtexec` FP16 engine built **on device**.  
- [ ] C++ detect loads engine; latency probe green.  
- [ ] No train job left filling `/data`.

---

## 8. Out of scope here

- Carrier HTE/FAE / GMSL bring-up (BOM / hardware audit).  
- Thermal camera path (V1.1).  
- Uploading weights to browsers / Vercel (forbidden — weights stay repo/Orin).
