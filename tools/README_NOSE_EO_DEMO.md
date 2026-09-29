# Nose EO demo sidecar (Mac / GCS)

Live **demo** detector for the ACTPROVE GCS **Nose EO** pane: YOLOv8n COCO on a local Python process, geometric closing-range estimate in meters, JSON for the Vite UI.

> **DEMO only.** Range is a pinhole estimate (`L=3.0 m` assumed target length, GoPro Webcam HFOV ≈ `120°`). It is **not** radar / LiDAR / stereo. Real class-gated Shahed / Geran / Gerbera weights are not used — HUD label is `TARGET (demo)`.

## Dependencies (Mac)

```bash
# Camera stack
# GoPro HERO13 → GoPro Webcam app → virtual camera "GoPro Webcam"

brew install python@3.12   # or use system python3
cd /path/to/actprove-drone

python3 -m venv .venv-nose-eo
source .venv-nose-eo/bin/activate
pip install ultralytics opencv-python fastapi uvicorn

# Weights: repo already ships vision/yolov8n.pt — or let ultralytics download yolov8n.pt
```

Optional OpenCV via Homebrew: `brew install opencv` (still need the pip package for `import cv2` in many setups).

If `vision/.venv` already has ultralytics + OpenCV:

```bash
source vision/.venv/bin/activate
pip install fastapi uvicorn   # if missing
```

## Run (Mac mini + GCS)

Terminal A — sidecar:

```bash
cd /path/to/actprove-drone
source .venv-nose-eo/bin/activate   # or vision/.venv
python tools/nose_eo_demo_server.py --port 8765

# Force camera index if needed:
# python tools/nose_eo_demo_server.py --camera-index 0
# NOSE_EO_CAMERA_INDEX=0 python tools/nose_eo_demo_server.py
```

Terminal B — GCS UI:

```bash
cd /path/to/actprove-drone/gcs-ui
npm install
npm run dev
```

`npm run dev` supervises both Vite and the browser-frame Nose EO sidecar. If either
process stops unexpectedly, the command exits instead of leaving a camera UI that
looks live but cannot detect or track. Use `npm run dev:ui` only when the sidecar is
already managed separately.

Open `http://127.0.0.1:5173/`, select **Nose EO**, allow camera (prefer **GoPro Webcam**), leave **Auto detect (demo)** on.

Smoke camera only (no YOLO):

```bash
python tools/gopro_uvc_smoke.py
python tools/gopro_uvc_smoke.py --list-only
```

## HTTP API (`http://127.0.0.1:8765`)

| Route | Role |
|-------|------|
| `GET /health` | Camera / detector status |
| `GET /detect` | Latest `{ok, boxes[{x,y,w,h,cls,conf}], range_m, closing, fps, …}` (boxes normalized 0–1) |
| `GET /mjpeg` | Annotated multipart JPEG (optional; UI normally uses browser `getUserMedia`) |
| `WS /ws` | Same JSON as `/detect` ~10 Hz |

CORS allows `http://127.0.0.1:5173` and `http://localhost:5173`.

### Range formula (DEMO)

```
focal_px = (frame_w / 2) / tan(hfov_deg / 2)
range_m  = (L * focal_px) / max(box_h_px, 1)
```

with `L = 3.0 m`, `hfov_deg = 120`. `closing=true` when range is decreasing.

Candidate COCO classes: `airplane`, `bird`, `kite`, `sports ball`, `person`. If none match, the largest confident box is used. If YOLO cannot load, OpenCV motion/contour fallback is used.

## Camera contention

Browser **Nose EO** `getUserMedia` and the sidecar both open the webcam. On some Macs / GoPro Webcam builds only one client can hold the device:

1. Prefer letting the **browser** own the preview; if `/health` reports camera errors, stop the browser stream briefly or set `--camera-index` to another device.
2. Or use sidecar `/mjpeg` as a fallback preview (operator tooling).

## Env / flags

| Flag / env | Meaning |
|------------|---------|
| `--port 8765` | Listen port (GCS default) |
| `--camera-index N` | Force OpenCV index |
| `NOSE_EO_CAMERA_INDEX` | Same as `--camera-index` |
| `--weights PATH` | Override `vision/yolov8n.pt` |
| `--conf 0.35` | YOLO confidence |

GCS UI override: `localStorage.setItem('noseEoDetectUrl', 'http://127.0.0.1:8765')` then reload.

## Visual accompany (`/track/*`) — DEMO

After operator Capture, GCS posts browser JPEG frames so the bbox **follows** the target (Mac-safe; OpenCV need not open the camera).

| Route | Role |
|-------|------|
| `POST /track/init` | Start CSRT/KCF/MIL/TMPL on ROI. Body: `{x,y,w,h,normalized?,jpeg_b64?,algo?}` |
| `POST /track/update` | Step tracker (+ preferred `jpeg_b64`) → `{ok,active,lost,box,algo,label}` |
| `GET /track/update` | Step using last camera JPEG (if any) |
| `GET /track` | Current DEMO track snapshot |
| `POST /track/stop` | Clear track |

`--no-camera` skips OpenCV `VideoCapture` (recommended when browser already holds GoPro Webcam).

Install `opencv-contrib-python` for CSRT/KCF; base `opencv-python` falls back to MIL then template matching.


## Track re-acquire knobs (Perception)

YOLO ROI re-acquire under fast closing (helpers in `nose_eo_track_helpers.py`):

| Knob | Default | Meaning |
|------|---------|---------|
| `ROI_EXPAND` / `ROI_EXPAND_BASE` | `2.5` | Base ROI pad around last box for YOLO re-acquire / TMPL search |
| Adaptive pad | up to `4.0` | `roi_expand_for_closing`: grows toward ~3.5–4.0 when area growth &gt;1.3 or `closing` |
| Area jump reject | `[0.4, 2.0]` | Reject re-acquire candidate if `cand.area / last.area` outside range (spike/collapse) |
| IoU accept | `≥0.12` (+ conf weight) | Score = IoU + 0.15×conf; pick best in ROI |
| `TRACK_LOST_FAILS` | `8` | Failures before DEMO lost (~0.5 s at ~16 Hz JPEG) |
| `TRACK_REDETECT_EVERY` | `12` | Periodic YOLO re-acquire cadence while tracking |

**Calib profile:** DEMO sidecar uses GoPro-class HFOV ≈120° (`DEMO_GOPRO` in `vision/configs/calib_profiles.yaml`). Never copy that FoV into `LOCK_CAM_A_40MM` (~10°) EST / Known-width. Labels stay `DEMO track` / `TARGET (demo)` — never class Lock.

Host test (no camera / weights):

```bash
python tools/test_nose_eo_tracking.py
```

## High-speed hold (Tracking policy)

See `docs/tracking/HIGH_SPEED_HOLD_POLICY.md`.

- Map DEMO lost fails to **0.5 s**: `TRACK_LOST_FAILS ≈ ceil(0.5 * jpeg_hz)`
- EST is DEMO soft range — clamp/reject if jump >2× per frame
- Flight Lock uses PPS `measurement_epoch`; DEMO is not flight Lock
