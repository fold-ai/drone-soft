# Click-to-lock + DEMO visual track (Nose EO) — 2026-09-17

## Goal
After **Capture** / MANUAL click-lock, the bbox must **continuously follow** the pointed target (visual accompany), not freeze at the click. Critical for moving targets (bottle now; Shahed-speed later).

## How to use
1. Start sidecar (browser frames preferred on Mac — avoids OpenCV camera TCC):
   ```bash
   cd actprove-drone
   source vision/.venv/bin/activate   # or .venv-nose-eo
   pip install fastapi uvicorn opencv-python   # opencv-contrib-python for CSRT/KCF
   python tools/nose_eo_demo_server.py --port 8765 --no-camera
   ```
2. GCS: `cd gcs-ui && npm run dev` → **Nose EO** → allow camera.
3. **ARM → WORK → SEARCH**. Prefer lock mode **MANUAL**.
4. Click / box the target → **Capture target** (MANUAL locks on click in SEARCH/LOCK).
5. HUD shows **DEMO track · accompany** — move the bottle; box follows.
6. If track drops: toast **DEMO track lost** → mission returns to **SEARCH**.

Capture still requires SEARCH/LOCK. In **BOOT**: toast says ARM → WORK → SEARCH first.

## Architecture
- **Browser** owns the live preview (`getUserMedia`).
- On Capture, GCS grabs JPEG frames from `<video>` and POSTs them to the sidecar:
  - `POST /track/init` `{x,y,w,h,normalized,jpeg_b64}`
  - `POST /track/update` `{jpeg_b64}` ~12 Hz → moving box
  - `POST /track/stop`
- Tracker preference: **CSRT → KCF → MIL → TMPL** (template NCC). Optional YOLO re-acquire in expanded ROI.
- Label is **DEMO track** (not class-gated Lock). Mission Track telem updates from bbox center while LOCK + vision hold.

## Files
- `tools/nose_eo_demo_server.py` — detect + `/track/*`
- `gcs-ui/src/hooks/useNoseEoTrack.ts` — browser→sidecar poll loop
- `gcs-ui/src/components/CameraFeed.tsx` — Capture starts track; redraws DEMO box
- `gcs-ui/src/hooks/useMissionSim.ts` — `updateTrackBox` / `setVisionHold` / `loseVisionTrack`
- `tools/README_NOSE_EO_DEMO.md`

Build: `cd gcs-ui && npm run build` (must PASS).
