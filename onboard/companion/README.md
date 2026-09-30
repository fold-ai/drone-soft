# ActProve companion — Lock / Takeover on Orin

Binary: `onboard/build/actprove_companion`

This is the process that may be loaded onto Orin **with UART to Pixhawk TELEM3**. TELEM1 is DJI, TELEM2 is the 3DR. It is not the camera-only observer.

```
pilot sticks ──► Pixhawk ──RC_CHANNELS + ATTITUDE──► companion
camera box  ──IPC──► companion ──ArduPlane GUIDED attitude──► Pixhawk
                 Lock CH7                              Takeover CH8 → FBWA
```

Plane only. Lock switches to ArduPlane GUIDED (mode 15) and sends bank, pitch, and throttle. The two rear flaperons stay mixed on the Pixhawk (elevon functions 77/78 if they are the only surfaces). Companion does not drive servo PWM.

## RC map (1-based)

| Switch | Default channel | PWM | Effect |
|--------|-----------------|-----|--------|
| **Lock** | CH7 | ≥1700 | Latch. No box yet: slow ±12° bank search. Box in frame: bank and pitch to put it on the nose, throttle holds cruise, plane flies through. |
| **Takeover** | CH8 | ≥1700 | Clears latch, SET_MODE FBWA (5), sticks live for the landing |
| Abort (optional) | `--abort-ch 6` | ≥1700 | Plane RTL (mode 11) + clear latch |

Lock stays latched until Takeover. Releasing the Lock switch does not give the sticks back.

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
  --lock-ch 7 --takeover-ch 8 --throttle 0.08 --bank 25 --hfov 10 --known-width 0.35

# 2) one JAI camera publishes the sky box
python3 /opt/drone-soft/tools/first_test_hdmi_preview.py \
  --headless --target sky --hfov 10 \
  --ipc /run/actprove/detections.sock
```

One camera only. Companion takes the single box nearest the crosshair.
