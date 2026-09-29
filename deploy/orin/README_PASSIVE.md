# Orin passive vision deployment

This deployment target is deliberately camera-only. It does not expose UART,
SBUS, PWM, GPIO, MAVLink command, or flight-controller devices to the service.
It is suitable for bench and non-contact observation tests only.

## Transfer source, not host artifacts

Do not copy `.venv-nose-eo`, `gcs-ui/node_modules`, or any existing `build/`
directory from macOS/x86 Linux. Native extensions and binaries must be rebuilt
on the Jetson ARM64 host.

Suggested source layout on Orin:

```text
/opt/drone-soft/
  vision/
  interfaces/
  deploy/orin/
  tools/
```

## Build on Orin

Install the JetPack-compatible CMake/compiler/TensorRT development packages,
then configure the vision tree with the real backend:

```bash
cd /opt/drone-soft
cmake -S vision -B vision/build -DSOFT_NO_JETPACK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build vision/build --parallel
```

Place the locally exported, JetPack-compatible engine at:

```text
/opt/drone-soft/vision/models/engines/yolov8n_fp16.engine
```

An engine is specific to the TensorRT/JetPack/GPU environment. Do not copy an
engine produced for a different TensorRT version or architecture.

Build it on the Orin from a reviewed ONNX artifact:

```bash
python3 tools/build_tensorrt_engine.py \
  --onnx vision/training/exports/smoke_e2.onnx \
  --engine vision/models/engines/yolov8n_fp16.engine
```

For a dynamic-shape ONNX, also pass `--input-name images --shape 1x3x640x640`.
The wrapper refuses non-Linux/non-ARM64 hosts and writes hashes, TensorRT
version, build log, and an engine metadata sidecar.

## Fail-closed readiness check

```bash
python3 tools/orin_passive_readiness.py
```

For an initial single-camera bench only:

```bash
python3 tools/orin_passive_readiness.py --allow-single-camera
```

The check intentionally fails while calibration, engine, camera devices, ARM64
binary, or build tools are absent. A PASS is not flight evidence.

## Service installation

After the readiness audit passes, install the camera-only unit:

```bash
sudo install -m 0644 deploy/orin/systemd/actprove-passive-vision.service /etc/systemd/system/
sudo install -m 0644 deploy/orin/systemd/actprove-passive-detection-logger.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now actprove-passive-detection-logger.service
sudo systemctl enable --now actprove-passive-vision.service
journalctl -u actprove-passive-vision.service -f
```

The `actprove` user must exist and belong to the `video` group. The unit is
restricted to `/dev/video0` and `/dev/video1`; it cannot access the FC UART.
Before every start, systemd runs the readiness audit in `--runtime-only` mode.
The service permits a SEARCH-only bench when `/dev/video1` is absent, but still
refuses to launch when the SEARCH camera/calibration, engine, or ARM64 binary is
missing. TELE remains a warning until the second calibrated camera is present.

Detections cross the process boundary as versioned little-endian Unix datagrams
at `/run/actprove/detections.sock`. The logger validates every datagram before
appending JSONL to `/var/lib/actprove/detections.jsonl`. Neither service has
access to a serial, RC, PWM, or GPIO device. Wire layout and rejection rules are
documented in `docs/icd/detection_ipc_v1.md`.

## Exit criteria before any outdoor observation

- both camera profiles are calibrated on the received units;
- the ARM64 observer stays up under systemd for the complete bench duration;
- measured FPS and end-to-end latency are recorded rather than assumed;
- camera disconnect/reconnect and process restart are tested;
- all observations are non-contact and no flight-control commands are emitted.
