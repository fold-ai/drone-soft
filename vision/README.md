# vision/ — Perception V1 (detect + box only)

**Hot path:** C++ / CUDA / TensorRT · **Python:** `tools/` only (not in 60 Hz path)  
**V1 G1 camera:** **EO only** (single cam). Thermal deferred to **V1.1** — `CamId::THERMAL` stays in the ICD enum but is unused.  
**Capture:** V4L2 primary for the selected ELP USB3 cameras; Argus reserved for a future CSI sensor · Ground EO USB/V4L2 for GCS Manual Lock  
**ICD:** `DetectionMsg` → Tracking (SPSC shm primary / ZMQ fallback) — layout locked; ClassId extended for range test  
**G1 gates:** ≥60 Hz sustained · frame→box p95 <40 ms · PPS on every box

Authoritative map: `docs/notes/vision_repo_map.txt`

```
nose EO (Argus/V4L2) ──┐
                       ├── detect / manual designate → DetectionMsg (cam_id=EO)
ground EO (GCS USB) ───┘     cam_id=2 on wire; source_role optional meta
```

## Classes (V1 range-test Lock gate)

| id | name |
|----|------|
| 0 | `shahed_136` |
| 1 | `geran_2` |
| 2 | `gerbera` |

Legacy `quad` / `fixed_wing` are optional in docs only — **not** in the V1 range-test Lock list.  
See `configs/classes_range_v1.yaml` and `notes/RANGE_TEST_DETECT.md`.

## Dataset quick start

```bash
# Drop photos into inbox by class, then ingest:
#   data/inbox/shahed_136/*.jpg
#   data/inbox/geran_2/*.jpg
#   data/inbox/gerbera/*.jpg
python3 tools/dataset/ingest_raw.py --inbox data/inbox --out data/raw
python3 tools/dataset/label_qc.py --labels data/raw
python3 tools/dataset/split_sets.py --root data/raw --out data/manifests
python3 tools/train/launch_train.py --epochs 5 --device cpu   # or --dry-run
```

## Ground EO (GCS laptop USB/V4L2)

- Config: `configs/capture_ground_eo_usb.yaml` (`cam_id: 2` = `CamId::GroundEo` on wire)
- Node stub: `apps/ground_eo_node` (`--manual` for designate path)
- Spec: `notes/GROUND_EO_MANUAL_LOCK.md`, `notes/GROUND_LOCK_HANDOFF.md`
- GCS toggle Nose EO | Ground EO: see `docs/notes/GROUND_EO_CUE_AND_BDA.md`
- Wire CamId: Nose EO=0, THERMAL=1 (V1.1), **GroundEo=2**

## Build

```bash
cmake -S . -B build -DSOFT_NO_JETPACK=ON   # host scaffold
cmake --build build -j
```

One-camera Jetson bring-up (kept working while TELE is absent):

```bash
./build/detect_node --device /dev/video0 --engine models/engines/yolov8n_fp16.engine
```

`detect_node` runs until `SIGINT`/`SIGTERM`. Add `--frames N` for a finite
bench/CI run; `--frames 0` explicitly selects continuous operation. Repeated
capture/inference failures exit non-zero for systemd restart, and periodic
`METRIC` lines report FPS and mean inference latency.

Dual-camera capture check; omitting or disconnecting TELE intentionally falls
back to SEARCH-only unless `--require-tele` is supplied:

```bash
./build/dual_capture_bench --search /dev/video0 --tele /dev/video1
```

Full SEARCH+TELE inference uses one TensorRT engine sequentially, with the same
SEARCH-only fallback:

```bash
./build/dual_detect_node --search /dev/video0 --tele /dev/video1 \
  --engine models/engines/yolov8n_fp16.engine --frames 0
```

Use YUYV for the TensorRT path. MJPEG capture is accepted by V4L2 but requires
a decoder and is therefore rejected by the current deterministic preprocessor.

Camera calibration and reconnect/FPS validation are documented in
`docs/bringup/passive_camera_validation.md`.
