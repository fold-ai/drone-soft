# ActProve companion — Lock / Takeover on Orin

Binary: `onboard/build/actprove_companion`

This is the process that may be loaded onto Orin **with UART to the Cube/ArduPilot TELEM2**. It is not the camera-only observer.

```
pilot (12ch RX) ──► FC ──MAVLink RC_CHANNELS──► Orin companion
cameras ──► dual_detect_node ──IPC──► companion ──GUIDED vel──► FC
                 Lock CH7 latch                  Takeover CH8 always wins
```

## RC map (1-based)

| Switch | Default channel | PWM | Effect |
|--------|-----------------|-----|--------|
| **Lock** | CH7 | ≥1700 | Latch: Orin takes GUIDED and flies at the box |
| **Takeover** | CH8 | ≥1700 | Clears latch, SET_MODE STABILIZE, sticks live |
| Abort (optional) | `--abort-ch 6` | ≥1700 | RTL + clear latch |

Lock is latched: a momentary button still holds intercept until Takeover.

## Build (Orin ARM64)

```bash
cmake -S onboard -B onboard/build -DCMAKE_BUILD_TYPE=Release
cmake --build onboard/build -j$(nproc) --target actprove_companion
```

Host smoke (no FC):

```bash
cmake -S onboard -B onboard/build
cmake --build onboard/build -j --target actprove_companion test_companion_app
ctest --test-dir onboard/build --output-on-failure
./onboard/build/actprove_companion --smoke
```

## Run on Orin

Companion **binds** `/run/actprove/detections.sock`. Do **not** run `actprove-passive-detection-logger` at the same time.

```bash
# 1) companion first (creates the socket)
./onboard/build/actprove_companion \
  --port /dev/ttyTHS1 --baud 57600 \
  --ipc /run/actprove/detections.sock \
  --lock-ch 7 --takeover-ch 8 --speed 18 --hfov 10 --known-width 0.35

# 2) vision publishes boxes
./vision/build/dual_detect_node \
  --search /dev/video0 --tele /dev/video1 \
  --engine /opt/drone-soft/vision/models/engines/yolov8n_fp16.engine \
  --frames 0 --ipc /run/actprove/detections.sock
```

Until a dedicated `test_drone` TensorRT engine exists, companion defaults to **any class** that passes box geometry (operator already pointed the nose). Rebuild the engine on Orin; do not copy a desktop `.engine`.
