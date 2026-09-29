# GCS video source + operator BDA (integration contract)

**Owner:** System stack (glue) · GCS UI · Perception (USB ingest) · Safety (SM) · Comms (MAVLink)  
**Status:** V1 range-test contract  
**Related:** `docs/notes/GROUND_EO_CUE_AND_BDA.md`, `docs/icd/ground_eo_handoff.md`, `docs/icd/mavlink_v1.md`

## Video source switch (Nose EO | Ground EO)

| Source | Host | Path | `cam_id` |
|--------|------|------|----------|
| **Nose EO** | Orin | GMSL / Argus (`vision/capture` Argus path) | **0** |
| **THERMAL** | Orin | Deferred **V1.1** | **1** |
| **Ground EO** | **GCS laptop** (NOT Orin nose) | USB / V4L2 `vision/capture` | **2** (`CamId::GroundEo` / `kGroundEo`) |

- GCS UI toggle is **UI + cue routing only** until live capture is wired.
- Ground EO capture defaults: device `/dev/video0`, **30 fps**, `cam_id=2`.
- Config stubs (Perception-owned ingest):  
  `vision/configs/capture_ground_eo_usb.yaml`  
  `vision/configs/capture_ground_eo_v4l2.yaml` (alias)
- ICD enum lives in `vision/iface/.../box_message.hpp` and `interfaces/include/actprove/detection_msg.hpp` — **do not redefine**.
- Manual Lock on Ground EO = **image box** → `DetectionMsg` with `cam_id=2` (never map-pin alone).
- GCS emits `GroundCueMsg` / BDA; Perception owns USB ingest.

## Operator BDA buttons

| GCS control | Uplink | USER_1 param1 | Mission effect |
|-------------|--------|--------------:|----------------|
| **Mark destroyed** | `TARGET_DESTROYED` | **3** | Clear engagement → **RTB always** (even if `TEST_RECOVER` armed). **Never LAND_SOFT** on this path. |
| **Mark miss / reattack** | `MISS/REATTACK` | **4** | Clear CLOSE → **ABORT**; allow new **WORK**. |

- Frozen wire: **no param1=5**. UX labels MISS and REATTACK share param1=4 / string `MISS/REATTACK`.
- Bridge APIs: `send_target_destroyed()`, `send_miss()`, `send_reattack()` in `onboard/mavlink_bridge/` (reattack ≡ miss on the wire).
- GCS C2 may originate on **TELEM1**; Orin bridge accepts/forwards into `safety_gates` as exact strings `TARGET_DESTROYED` / `MISS/REATTACK`.
- `LAND_SOFT` only on **automatic** post-CLOSE miss when `TEST_RECOVER` was set **before WORK** — not after BDA destroy.
- After CLOSE completes into ABORT (with or without LAND_SOFT), operator BDA remains available until Mark destroyed/miss or RTB/FTS.

## Black-box

- `bda.jsonl`: `{publish_time_ns, cmd, mission_state, video_source}`
- optional `gcs_events.jsonl`: `video_source_changed` with `video_source: nose|ground`

## Owners

| Piece | Owner |
|-------|-------|
| GCS Nose\|Ground toggle + BDA buttons (UI) | GCS / System stack |
| USB/V4L2 Ground EO ingest | Perception (`vision/capture`) |
| `GroundCueMsg` → track handoff | Tracking (+ Navigation bearing) |
| BDA → RTB / ABORT SM | Safety (`onboard/safety_gates`) |
| MAVLink USER_1 BDA uplink + bridge | Comms + System stack |
| Black-box `bda` / `gcs_events` streams | System stack (`onboard/logging`) |
| Integration ICD (this doc) | System stack |

## Non-goals (V1)

- Autonomous kill / combat intercept.
- Video on MAVLink.
- Redefining `CamId` in stack glue.
