# First-test stack — JAI GOX-5103M-USB + Pixhawk 6C + TBS

**Stamp:** 2026-09-21  
**First air goal:** operator points the nose, Orin draws a box on HDMI, latch Lock on a **weather balloon** or a **small drone**. Not a closing intercept yet.

## What you already bought (keep)

| Item | Role in first test |
|------|--------------------|
| **Pixhawk 6C** | FC. Flash **ArduCopter** (quad) or ArduPlane (FW). TELEM2 → Orin. |
| **TBS Tango 2 + Nano RX** | Crossfire sticks + **CH7 Lock / CH8 Takeover**. Companion reads `RC_CHANNELS` from the FC, so Crossfire is fine (ELRS not required). |
| **3DR 433 MHz radio** | Thin MAVLink to Mission Planner. **Cannot carry video.** |
| **GPS** | FC GPS. Do not share with Orin for this test. |
| **DJI O4** | **Pilot FPV only.** It will never show YOLO / Lock boxes. |
| **JAI GOX-5103M-USB** | Nose compute camera. USB3 Vision, Sony IMX264, **2448×2048**, global shutter, ~35 fps, C-mount, ~4.3 W, PoUSB. |
| **Kowa LM50HC 50 mm** | Lock optic. On this 2/3" sensor **HFOV ≈ 10°** (VFOV ≈ 7.5°). |
| Orin (incoming) | Runs preview + later companion. |

At 100 m the 10° frame is ~17 m wide. A 1.2 m balloon is a large box. A 0.35 m quad is ~50 px — still lockable if you point well. At 200 m a small drone is ~25 px; balloon is still easy.

## Must buy for the first test (do this list)

Do **not** wait on Forecr / GMSL / RFD900 / thermal.

| # | Item | Why | Notes |
|---|------|-----|--------|
| 1 | **Orin carrier with HDMI + USB 3.0** | Module-only Orin cannot show a monitor or talk to the JAI | NVIDIA DevKit, Seeed J401, Waveshare, Auvidea — **USB3 is non-negotiable**. Cheap USB2 hubs will drop the camera. |
| 2 | **Orin cooler / heatsink fan** | 15–30 W sustained, more with USB cam + infer | Comes with some kits; buy if the module is bare. |
| 3 | **HDMI monitor + HDMI cable** | This is how you **see Lock** on the bench | Plug into the carrier **before** boot. First-test HUD is `tools/first_test_hdmi_preview.py`. |
| 4 | **USB 3.0 SuperSpeed cable** | JAI GOX USB port is typically **USB3 Micro-B** | Use a short, thick, **SS** cable (blue tongue). Phone USB2 cables will not work. |
| 5 | **Pixhawk TELEM2 ↔ Orin UART cable** | Companion MAVLink | Holybro 6-pin JST-GH. **3.3 V UART, common ground, do not connect 5 V into Orin TX.** Baud **57600**. |
| 6 | **Orin power from the pack** | Pixhawk **cannot** power Orin | 9–19 V class, **≥5 A / ~60 W** UBEC or the carrier’s barrel/XT30 input. Separate from servo BEC. |
| 7 | **Laptop + Mission Planner / QGC** | 3DR 433 C2, mode, logs | You likely already have this. |
| 8 | **Weather balloons + helium** | First cooperative target | Bright / high-contrast. `--known-width 1.2`. |
| 9 | **MicroSD or NVMe** | JetPack + overlay recordings | Whatever the carrier boots from. |
| 10 | **3D-printed / plate mount** | Rigid C-mount on the airframe, looking forward | Lens is 210 g + camera 65 g ≈ **275 g** on the nose. Check CG. |

### Buy only if the first plug-in fails

| Item | When |
|------|------|
| **Powered USB3 hub** (or JAI 6-pin Hirose **+12 V**) | Camera enumerates then drops / USB resets. PoUSB is 4.3 W; some Orin ports brown out. |
| **USB3 extension with locking screws** | Vibration in the air. |
| **Second wide USB camera (~90°)** | 10° is a lock optic. Finding a small drone without pointing is hard. **Not required** for balloon-first if the pilot aims with O4. |
| **IR-cut / ND** | Harsh sun; stop down the Kowa to ~F4–F8 first (free). |

## Do not buy for this gate

- Second compute, GMSL deserializer, Forecr, Basler GMSL, RFD900 (you have 3DR 433), analog 5.8 as the compute camera, HDMI converter into DJI O4 (will not overlay boxes).

## How you see what Orin is doing

```
JAI ──USB3──► Orin ──HDMI──► monitor   ← boxes, LOCK text, range (BENCH)
                 └── local .mp4 recording   ← review after a hop (AIR)

Pilot O4 ──► goggles     ← fly the aircraft only
3DR 433  ──► laptop      ← modes / HEARTBEAT only, no picture
```

**Bench:** HDMI monitor on the Orin. Red box = candidate, thick red + `LOCK` = tracker hold.

**Air:** you will **not** see the HUD in the O4 goggles. Record overlay on Orin (`--record`), land, play the file. A later FPV overlay into O4 is a different project.

## Software first test (when Orin + JAI are on the desk)

```bash
# Orin desktop, monitor already plugged in
sudo apt-get install -y python3-opencv python3-gi gir1.2-aravis-0.8
export DISPLAY=:0
python3 /opt/drone-soft/tools/first_test_hdmi_preview.py \
  --hfov 10 --known-width 1.2 --target balloon
```

`--target drone` uses 0.35 m. `--webcam 0` is a laptop UVC fallback. JAI is **USB3 Vision**, not `/dev/video0`; Aravis is required on Orin.

Map Tango 2: **CH7 = Lock**, **CH8 = Takeover**. Load `flight/params/ardupilot_companion.params` after the HDMI lock looks stable.
