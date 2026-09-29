# Nose EO fast-close — System stack land notes

**Stamp:** 2026-09-17 CT · **Owner:** System stack (GCS + demo sidecar glue)  
**Status:** TEST CONDITIONAL unlock paths landed — **not** Lock PASS.

## Routes (`tools/nose_eo_demo_server.py`)

| Route | Role |
|-------|------|
| `GET /health` | Sidecar / track readiness |
| `GET /detect` | Latest detect snapshot (boxes, EST, lab evidence) |
| **`POST /detect/frame`** | Raw `image/jpeg` body from Mac GCS → decode → YOLO/motion → EST |
| `GET /mjpeg` | Annotated MJPEG (optional) |
| `WS /ws` | Detect JSON stream |
| `GET/POST /config` | Calib profile + EST knobs |
| `POST /calib/hfov` | Calibrate HFOV at known range (per active profile) |
| `POST /track/init\|update`, `GET /track`, `POST /track/stop` | DEMO visual accompany |

### `POST /detect/frame` headers

| Header | Meaning |
|--------|---------|
| `Content-Type: image/jpeg` | Raw JPEG bytes in body |
| `X-Target-Size-M` | Known width (m) for monocular EST; else active `known_width_m` |
| `X-HFOV-Deg` | Optional HFOV override — **subject to profile rule below** |

Response is compatible with `GET /detect` and always includes DEMO labels plus lab keys:

- `label: "TARGET (demo)"`, `demo: true`, `lock_eligible: false`
- `jpeg_hz`, `est_jump_ratio`, `range_status`
- `profile_id`, `hfov_deg`, `range_m`, `closing`, `boxes`

## Profile HFOV hard rule

Profiles: `vision/configs/calib_profiles.yaml`

| Profile | Default HFOV | Notes |
|---------|--------------|-------|
| `DEMO_GOPRO` (default) | ~120° | Lab Mac GoPro Webcam only |
| `LOCK_CAM_A_40MM` | ~10° | Flight Cam A path — **never** inherit GoPro FoV |

**Rule:** when active profile is `LOCK_CAM_A_40MM`, do **not** use GoPro ~120° HFOV for EST.

- Prefer profile `hfov_deg` (~10°) or a calib override already stored on that profile.
- `X-HFOV-Deg` may override only when the header is **narrow** (`< 60°`). GoPro-wide headers (`≥ 60°`) are **ignored** (`hfov_source=profile_ignored_gopro_header`).
- `POST /config` similarly rejects parking `hfov_deg ≥ 60` onto `LOCK_CAM_A_40MM`.

`DEMO_GOPRO` allows header override freely (lab).

## `jpeg_hz` + `est_jump_ratio` logging (GCS)

Sidecar reports measured values on:

- track snapshots (`GET /track`, `/track/update`)
- detect snapshots (`GET /detect`, `POST /detect/frame`)

GCS UI (`CameraFeed` HUD + lab cue) surfaces **sidecar-reported** `jpeg_hz` and `est_jump_ratio` (not only localStorage knobs). Calib profile selector: `DEMO_GOPRO` (default) vs `LOCK_CAM_A_40MM`. Click-to-lock / accompany cues stay **DEMO**-labeled.

## Smoke

```bash
# host (venv with fastapi + opencv)
python tools/test_nose_eo_detect_frame.py

# or curl
# curl -s -X POST http://127.0.0.1:8765/detect/frame \
#   -H 'Content-Type: image/jpeg' -H 'X-Target-Size-M: 0.08' -H 'X-HFOV-Deg: 120' \
#   --data-binary @frame.jpg
```

## Non-claims

- No Lock PASS / no class-gated Lock.
- EST is monocular DEMO — SEARCH bias only; not radar/LiDAR.
- No buys.
