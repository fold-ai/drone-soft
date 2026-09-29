# Passive camera validation

These procedures validate camera geometry and runtime health only. They do not
connect to a flight controller or emit control commands.

## 1. Record checkerboard views

For each physical lens/zoom setting, record at least 15–25 sharp images. Move
the checkerboard across the full frame and vary distance and tilt. Do not mix
SEARCH and TELE images or change TELE zoom within one set.

Example layout:

```text
calibration/
  elp_search_100deg/
  elp_tele_50mm/
```

## 2. Solve intrinsics offline

For a 9×6 inner-corner board with 25 mm squares:

```bash
python3 tools/calibrate_elp_checkerboard.py calibration/elp_search_100deg \
  --cols 9 --rows 6 --square-mm 25 --min-views 12 \
  --out calibration/elp_search_100deg.json

python3 tools/calibrate_elp_checkerboard.py calibration/elp_tele_50mm \
  --cols 9 --rows 6 --square-mm 25 --min-views 12 \
  --out calibration/elp_tele_50mm.json
```

The tool rejects duplicate, undecodable, wrong-resolution, and no-board images.
`READY_FOR_REVIEW` means only that the numerical solve met the configured RMS
limit. It does not automatically set `calibrated: true` in the flight profile.

## 3. Camera FPS and reconnect bench

Jetson V4L2 example:

```bash
python3 tools/camera_reconnect_bench.py --device /dev/video0 \
  --duration 120 --expected-fps 30 --min-fps 25 \
  --out calibration/search_camera_bench.json
```

During a dedicated bench run, briefly unplug/replug the camera once to exercise
reopen behavior. The JSON records measured FPS, read-latency percentiles,
failures, reconnect attempts, and estimated dropped frames.

## 4. Passive observer

`dual_detect_node` runs continuously by default and accepts `--frames N` for a
finite test. It prints periodic `METRIC` lines with inference FPS, mean inference
latency, and reconnect count. Repeated failures return non-zero so systemd can
restart the camera-only service.

## Acceptance evidence

Keep the following together for each physical camera/lens configuration:

- checkerboard source images;
- calibration JSON and RMS review;
- reconnect bench JSON;
- observer journal covering the full bench duration;
- exact JetPack, TensorRT, engine hash, camera serial, resolution and FPS.

## Dataset integrity gate

Before any offline model evaluation, verify that every manifest entry resolves
to an image and YOLO label, label coordinates are normalized, and train/val/test
do not share paths or image hashes:

```bash
python3 tools/dataset_integrity_audit.py --require-ready
```

This gate checks file and split integrity only. Even a passing report still
requires independent human review of label accuracy and domain coverage.
