# SYSTEM STATUS — actprove-drone + actprove-ops

**Date:** 2026-09-17 (America/Chicago)  
**Method:** Filesystem read of `/workspace/actprove-drone` and `/workspace/actprove-ops` (no interceptor code changes from ops).  
**Boundary:** `actprove-ops/BOUNDARY.md` — ops must never modify `actprove-drone`. This audit is written under `actprove-drone/docs/audit/`; optional pointer: `actprove-ops/docs/SYSTEM_STATUS_POINTER.md`.

---

## A. actprove-drone — what exists

### Top-level map

| Path | Role |
|------|------|
| `vision/` | Perception: Argus/V4L2 capture scaffolds, detect, training, Label Studio, YOLO weights |
| `tracking/` | Detection → track → Lock + Ground EO handoff (C++ smoke + virtual tests built) |
| `navigation/` | Heading / commanded-miss geometry → MAVLink setpoints (`navigation_smoke` built) |
| `onboard/` | System stack: `mavlink_bridge`, `safety_gates`, `time_sync`, `logging`, `bringup` |
| `interfaces/` | MAVLink headers (`mavlink_v1_cmds.h`, `mission_state.h`, …), JSON schemas, ROS2 msg stubs |
| `flight/` | Airframe iface, `params/v1_telem.params`, FS-03 Lua `scripts/companion_hb_rtl.lua` |
| `gcs-ui/` | Vite/React local GCS demo (canvas EO + mission sim) — **not** live video |
| `docs/` | ICD, hardware, safety, comms, bringup, integration, prior audits |
| `test/` | G1–G3 / range / recognition gate cards |
| `tools/` | `latency_probe`, `log_replay`, `extrinsics_calib` |
| `sim/` | ArduPilot SITL notes (scaffold) |
| Root | `README.md`, `V1_PLAN.md`, `BOM-v0.1.md`, `BUY_LIST.md`, `NOTES.md`, `SYNC.md` |

Perception lives under `vision/` (no separate top-level `perception/`).

### Perception / training readiness

| Item | Status on box (2026-09-17 CT) |
|------|-------------------------------|
| `vision/training/datasets/label_batch_v1` | **120** images (40/class), **120 empty** YOLO `.txt` labels — ready for Label Studio |
| Label Studio | **Listening** `0.0.0.0:8080` (`vision/training/labelstudio/start_label_studio.sh`) |
| Smoke weights | `vision/yolov8n.pt` (~6.2 MiB); `vision/training/exports/smoke_e2.onnx` (~11.7 MiB); run dir `vision/training/runs/smoke_e2/` |
| Python stack | `vision/.venv`: Ultralytics **8.4.154**, OpenCV **5.0.0** |
| C++ capture/detect | Built: `vision/build/ground_eo_node`, `libvision_capture.a`, `libvision_detect.a` (soft JetPack) |
| Ground EO config | `vision/configs/capture_ground_eo_usb.yaml` → `/dev/video0`, 30 fps, `cam_id=2` (also `capture_ground_eo_v4l2.yaml`) |
| Production QC | **Not ready** — `vision/training/docs/STATUS.md`: provisional/empty labels; do not claim QC PASS |

### Safety / nav / comms — scaffold vs production

| Subsystem | On disk | Production-ready? |
|-----------|---------|-------------------|
| `onboard/safety_gates` | Mission SM BOOT…FTS; Lock⇒kill vs TEST_RECOVER; lib built | **SW scaffold** — policy frozen in docs; not airframe-proven |
| `navigation/` | Miss geometry, heading guidance, soft-land; `navigation_smoke` | **Conditional** — logic smoke; ADIS DRDY→HTE HW HOLD |
| `tracking/` | Tracker + Ground EO handoff; `tracking_smoke` + virtual tests | **SW PASS** (prior audit); no live camera |
| `onboard/mavlink_bridge` | Soft UART smoke; BDA send stubs | **Partial** — stub; no live TELEM2 on this host |
| Comms ICD | `interfaces/mavlink/*` + near-field video envelope docs | C2 dict OK; **near-field video RF P/N TBD** |
| FS-03 | `flight/scripts/companion_hb_rtl.lua` + logic twin under `docs/integration/FS03_COMPANION_HB.md` | **Logic/Lua PASS**; HW/SITL bring-up still open |

### Hardware / ICD freezes relevant now

| Freeze | Where |
|--------|-------|
| **FC:** Cube Orange + ArduPilot (Pixhawk 6X/PX4 = ALT only) | `BUY_LIST.md`, `docs/SYSTEM_STACK.md`, supplier-adapt docs |
| **Cam A:** Basler **a2A1920-168mgm** + **40 mm** C-mount (~10° HFOV); 50 mm ALT; 35 mm not primary Lock | `docs/hardware/HIGH_SPEED_EO_HW.md`, `docs/tracking/CAM_A_FOCAL_STAMP.md` |
| **Lock ⇒ kill** (combat); BDA = post-engage bookkeeping; TEST_RECOVER for surrogates | `docs/notes/LOCK_EQUALS_KILL.md` |
| **FS-03:** Orin HEARTBEAT **compid 191** silence ≥ **3.0 s** → RTL; never auto-FTS; soft-land suppress via `SCR_USER1` | `docs/integration/FS03_COMPANION_HB.md` |
| TELEM1 = RFD900 C2; TELEM2 = Orin companion @ **57600**; **no video on MAVLink** | ICD + `flight/params/v1_telem.params` |
| Near-field video default band **2.4 GHz** (SKU TBD) | `docs/comms/NEAR_FIELD_VIDEO_ENVELOPE.md` |

### Gaps / not wired yet

- No `/dev/video*` on this audit host.
- `vision/capture/src/v4l2_capture.cpp`: soft-open + **synthetic** `grab()` (`device_ptr = nullptr`, always `return true`).
- Argus/GMSL/TensorRT engines: stubs; `vision/models/engines/` empty — needs Orin JetPack.
- Forecr carrier FAE (HTE + PPS/FSYNC + mass) still blocks carrier PO / Integration PASS.
- FTS holder unnamed; pack V unknown.
- `gcs-ui` not wired to live capture or live MAVLink.
- Ground EO → DetectionMsg → Tracking → Nav → TELEM2 not closed on hardware.

---

## B. actprove-ops — what exists

### Layout

| Path | Role |
|------|------|
| `cloud/web/` | Next.js ops GCS (Vercel project `web`, id `prj_DPiR0XIREQDU3s1IznAbtka26B4d`) |
| `cloud/supabase/` | Migration `migrations/0001_ops_gcs.sql` — `missions`, `mission_events`, `telemetry_latest`, `operator_sessions` + RLS |
| `cloud/DRONE_CONNECT.md` | Browser → Realtime → **ground gateway** → RFD900 MAVLink |
| `sim/` | Browser virtual training: detect → lock → intercept / miss / BDA |
| `tower/` | EO tower concept, configs, BOM, FOV (docs-heavy) |
| `interfaces/` | `CHANNEL_MAP.md`, `CLOUD_TO_AIRCRAFT.md`, `tower_detect_msg.md` |
| `chips/` | Orin, Basler, RFD900, ADIS16470, … reference cards |
| `docs/` | `PROGRAM.md`, `CONNECTIVITY.md`, `SAFETY_OPS_CLOUD_TOWER.md` |
| `BOUNDARY.md` | **Never modify actprove-drone from ops work** |

### Live vs stub

| Piece | Status |
|-------|--------|
| Vercel / Next web | Scaffold + `.next` build artifacts; offline mission sim if Supabase env unset |
| Supabase schema | SQL ready (`APPLY_NOW.md` / dashboard apply) |
| Video panel | **Stub** — `cloud/web/components/VideoToggle.tsx` + `MissionPanel.tsx`: “Video stub — live EO is not carried on MAVLink / RFD900 C2” |
| Ground gateway | **Missing** — no `cloud/gateway/` dir; `DRONE_CONNECT.md` §3 = implement later |
| Tower detect | Docs + `tower/configs/capture_tower_eo.yaml` (`device: ""`, comments allow `/dev/video0`) — not a live service here |
| Boundary | Ops may **read** drone ICDs; all ops writes stay under `actprove-ops/` |

---

## C. End-to-end data paths

```
 Nose EO (Basler a2A1920-168mgm)
   GMSL2/FAKRA ──► Orin NX (detect + track + safety SM)
                      │
                      ├── UART TELEM2 MAVLink @ 57600 ──► Cube Orange (ArduPilot)
                      │                                      │
                      ├── HEARTBEAT compid 191 (FS-03)       ├── TELEM1 ◄──► RFD900 C2
                      └── NVMe black-box                     └── (GUIDED / RTL / cmds)

 RFD900 TELEM1: WORK / ABORT / RTB / FTS + thin telem ONLY — no HD video
 Near-field video RF (2.4 GHz class, SKU TBD) ──► GCS preview (separate from C2)
 Ground EO USB/V4L2 (/dev/video0, cam_id=2) on GCS laptop ──► cue / Manual Lock seed

 Cloud: Browser ──HTTPS──► Vercel web ──► Supabase Auth / Realtime / DB
                                              │
                                              ▼
                                    [Ground gateway — NOT IMPLEMENTED]
                                              │
                                              └──► RFD900 TELEM1 (same C2 plane)
```

### Code vs docs-only

| Path | Implemented in code | Docs-only / stub |
|------|---------------------|------------------|
| Nose EO → Orin detect | Capture/detect C++ scaffolds; soft no-JetPack | Live GMSL/Argus/TRT |
| Orin → TELEM2 → Cube | `mavlink_bridge` smoke + `v1_telem.params` | Live UART on aircraft |
| RFD900 TELEM1 C2 | Params + ICD | Live radio |
| Near-field video | — | Envelope + BUY class docs |
| Ground EO USB | Config + `ground_eo_node` (synthetic grab) | Real V4L2 pixels |
| gcs-ui / ops video | Canvas demo / stub UI | Live stream / `getUserMedia` |
| Supabase → gateway → RF | Schema + web UI / sim | **Gateway service absent** |
| FS-03 companion HB | Lua + logic twin | Cube HW/SITL proof |
| Lock⇒kill SM | `safety_gates` C | Airframe integration |

---

## D. Live camera / GoPro hook points

**Search (both trees, excluding `node_modules` / `.venv*` / `dist` / `.next`):**  
webcam, OpenCV `VideoCapture`, WebRTC, `getUserMedia`, GStreamer, MAVLink camera, video stub.

### Findings

| Mechanism | Present? | Where |
|-----------|----------|-------|
| `getUserMedia` / WebRTC in app source | **No** | Not in `gcs-ui/src` or `cloud/web` app code |
| OpenCV `cv2.VideoCapture` script in repo | **No dedicated webcam smoke script** | OpenCV **installed** in `vision/.venv` — usable ad hoc |
| GStreamer | **No** app usage found | — |
| MAVLink camera / video-on-C2 | **Forbidden** by ICD | — |
| Video stub (ops) | **Yes** | `actprove-ops/cloud/web/components/VideoToggle.tsx`, `MissionPanel.tsx` |
| Demo canvas “camera” (drone GCS) | **Yes** | `actprove-drone/gcs-ui/src/components/CameraFeed.tsx` |
| V4L2 `/dev/video0` hooks | **Yes (scaffold)** | See table below |
| Host `/dev/video*` today | **Absent** | Need UVC device |

### Concrete plug-in points (minimal glue)

| # | File / symbol | Today | GoPro (UVC) fit |
|---|----------------|-------|-----------------|
| 1 | `vision/configs/capture_ground_eo_usb.yaml` | `device: /dev/video0`, 30 fps, `cam_id: 2` | Point `device:` at GoPro UVC node |
| 2 | `vision/configs/capture_ground_eo_v4l2.yaml` | Same device default | Lab alternate |
| 3 | `vision/capture/src/v4l2_capture.cpp` — `V4L2Capture::open` / `grab` | Soft-opens; **synthetic frames** | Needs real V4L2 read before production |
| 4 | `vision/capture/src/ground_eo_capture.cpp` — `GroundEoCapture` | Forces `cam_id=2`, GroundEo role | Correct role for Ground EO smoke |
| 5 | `vision/apps/ground_eo_node/main.cpp` | 5-frame loop; `--manual` or detector | Entry binary after real grab works |
| 6 | `gcs-ui/src/components/CameraFeed.tsx` | Animated canvas; Nose/Ground toggle | Natural `<video>` + `getUserMedia` hook |
| 7 | `cloud/web/components/MissionPanel.tsx` + `VideoToggle.tsx` | Static stub copy | Same browser path for ops GCS |
| 8 | `tower/configs/capture_tower_eo.yaml` | `device: ""` (lab USB noted) | Tower lab only — not interceptor hot path |
| 9 | `vision/.venv` + `yolov8n.pt` / `training/exports/smoke_e2.onnx` | Ultralytics ready | **Fastest** pixels + boxes without C++ changes |

### Recommendation (smallest path)

User options: (1) browser `getUserMedia`, (2) Python OpenCV → YOLO, (3) both.

**Recommend (2) first, then optionally (1) → i.e. path (3) staged:**

1. **Python OpenCV → YOLO on the box (best TODAY)**  
   Plug GoPro in UVC mode → confirm `/dev/videoN` → short script in `vision/.venv`: `cv2.VideoCapture(N)` → `YOLO('yolov8n.pt')` or smoke ONNX.  
   Proves USB + NN smoke without Argus, MAVLink, or ops gateway. Aligns with Ground EO ownership.

2. **Browser `getUserMedia` into `gcs-ui` (or ops web)**  
   Replace canvas clear in `CameraFeed.tsx` (or ops stub) with `navigator.mediaDevices.getUserMedia({ video: true })`.  
   Good for operator “I see the feed” / Manual Lock UX — does not feed Orin/Tracking unless bridged.

3. **Both** once (2) works — keep video decoupled from C2.

**Do not** put GoPro frames on RFD900/MAVLink.

---

## E. Go / no-go — “connect GoPro and see if it works”

### CONDITIONAL GO — lab smoke (pixels ± YOLO) if GoPro is UVC

1. Connect GoPro (USB webcam / UVC) to the **box or GCS laptop** (not via RFD900).
2. Confirm: `ls -l /dev/video*` — **today none on this host** → blocked until plug-in.
3. Run OpenCV (± YOLO) in `vision/.venv` against that index.
4. Optional: set `capture_ground_eo_usb.yaml` `device:` (C++ `ground_eo_node` still needs a real V4L2 `grab` before it shows truth frames).

### NO-GO for “full system works”

| Blocker | Impact |
|---------|--------|
| No `/dev/video*` currently | Cannot validate capture on this host until UVC appears |
| `V4L2Capture::grab` synthetic | C++ Ground EO can “PASS” without real pixels |
| No `getUserMedia` in UIs | Browser path needs a small code change |
| No ops gateway | Cloud GCS cannot reach aircraft C2 |
| No near-field video RF SKU | Aircraft Nose EO preview to GCS unavailable |
| No Orin/JetPack/Basler on this box | Nose EO hot path cannot run here |
| Empty `label_batch_v1` labels | Class-gated Lock NN not production-trained |

### Verdict table

| Goal | Verdict |
|------|---------|
| Plug GoPro UVC → see frames (OpenCV) ± YOLO on box | **CONDITIONAL GO** — needs device node + ~minutes of Python glue |
| GoPro appears in gcs-ui / ops web with zero code | **NO-GO** — stub/canvas only |
| GoPro substitutes for Basler nose hot path / Lock⇒kill | **NO-GO** — wrong camera class, host, and path |
| Cloud → gateway → drone because GoPro works | **NO-GO** — gateway missing; video ≠ C2 |

---

## Pointers

- Ops one-line mirror: `actprove-ops/docs/SYSTEM_STATUS_POINTER.md`
- Prior channel audits: `docs/audit/PM_ROLLUP.md` and sibling `*_AUDIT.md` (2026-09-16). This file is the **2026-09-17 cross-tree + GoPro hook** status.

## F. Update (same day, post-audit)

- **Ground EO live USB:** `gcs-ui` now uses `getUserMedia` for Ground EO (GoPro Webcam Mode / UVC). Nose EO remains synthetic.
- Dev server on Mac mini: `http://127.0.0.1:5173/` under `Downloads/actprove-drone/gcs-ui`.
- Smoke helper: `tools/gopro_uvc_smoke.py`.
- Label Studio still on box `:8080`; `label_batch_v1` human boxes still the NN blocker.
