# Orin Lock / Takeover stack (what to flash)

**Stamp:** 2026-09-20  
**Goal:** something that can be copied to Orin today: cameras detect, CH7 Lock gives GUIDED to the box, CH8 Takeover returns ArduPilot sticks.

This replaces the old “passive observer only” path for intercept bring-up. Camera-only benches still use `deploy/orin/README_PASSIVE.md`.

## Flight sequence (pilot)

1. Arm and climb **manually** (STABILIZE / ALTHOLD / LOITER). Orin must not arm the vehicle.
2. Point the nose at the target.
3. **Lock (CH7 high)** — companion latches, sets ArduPlane **GUIDED** (mode 15). With no box it banks slowly to search. With a box it sends bank, pitch, and cruise throttle so the nose stays on the target. **Takeover (CH8)** returns **FBWA**.
4. **Takeover (CH8 high)** — latch clears, mode **STABILIZE**, sticks own the aircraft again. Takeover always wins if both switches are high.
5. Optional abort channel (`--abort-ch`) sends RTL.

## On the aircraft

| Piece | Role |
|-------|------|
| `actprove_companion` | Binds detections socket, reads `RC_CHANNELS` and `ATTITUDE`, SET_MODE GUIDED/FBWA, 15 Hz bank/pitch/throttle |
| `dual_detect_node` | USB cameras → TensorRT → IPC |
| Pixhawk TELEM3 @ 57600 (SERIAL5) | UART to Orin `/dev/ttyTHS1` (confirm with `dmesg`). TELEM1 is DJI, TELEM2 is 3DR. |
| ELRS 12ch RX | Pilot + Lock + Takeover into FC (not into Orin) |

Load `flight/params/ardupilot_companion.params` on the Cube (review first).

## Copy / build

From the Mac:

```bash
chmod +x tools/pack_orin.sh deploy/orin/install_companion.sh
./tools/pack_orin.sh actprove@<orin-ip>
```

On Orin (JetPack):

```bash
cmake -S /opt/actprove-drone/onboard -B /opt/actprove-drone/onboard/build -DCMAKE_BUILD_TYPE=Release
cmake --build /opt/actprove-drone/onboard/build -j$(nproc)
cmake -S /opt/actprove-drone/vision -B /opt/actprove-drone/vision/build -DSOFT_NO_JETPACK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /opt/actprove-drone/vision/build -j$(nproc)
python3 /opt/actprove-drone/tools/build_tensorrt_engine.py \
  --onnx /opt/actprove-drone/vision/training/exports/smoke_e2.onnx \
  --engine /opt/actprove-drone/vision/models/engines/yolov8n_fp16.engine
sudo /opt/actprove-drone/deploy/orin/install_companion.sh
sudo systemctl start actprove-companion actprove-vision
journalctl -u actprove-companion -u actprove-vision -f
```

Host check before copy:

```bash
cmake -S onboard -B onboard/build && cmake --build onboard/build -j
ctest --test-dir onboard/build --output-on-failure
./onboard/build/actprove_companion --smoke
```

## Still not done (do not treat METRIC CLOSE as a cleared intercept)

- Dedicated `test_drone` weights (current ONNX is a smoke net; COCO/shahed labels will miss a generic quad).
- SBUS synthetic RX from Orin (MAVLink GUIDED is the live path).
- Camera calibration / real HFOV (`--hfov`).
- Live TELEM3 proof on the desk (heartbeat + RC_CHANNELS in `journalctl`).
- Gain tuning (`--speed`, `kp`) on a tether / short hop before any closing pass.

Do not enable `actprove-passive-detection-logger` together with companion — both bind the same Unix socket.
