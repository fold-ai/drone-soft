# System stack notes (V1)

**Owner:** System stack · **Plan:** V1_PLAN.md §4–§5  
**Stamp:** 2026-09-16 CT

## Ownership map

- ICD + bringup docs, `interfaces/`, `onboard/{bringup,time_sync,logging,mavlink_bridge,safety_gates}`, `tools/{latency_probe,log_replay,extrinsics_calib}`
- Does **not** own: `vision/` ingest implementation, `tracking/`, `navigation/`, `test/`, `flight/` airframe/hardware, root BOM
- **Does** own stack glue for Ground EO + BDA: GCS integration contract, mavlink_bridge BDA uplink stubs, black-box `bda`/`gcs_events`, ICD docs under `docs/icd/`

## Hard ICD reflections in code

1. Track / black-box carry **`measurement_epoch`** (PPS); coast keeps last real measurement.
2. Orin↔FC = UART MAVLink on **TELEM2 @ 57600**; TELEM1 = RFD900x; no video on MAVLink.
3. `time_sync` before cameras; soft `now()` forbidden.
4. Latency probe budgets: cam→Orin ≤30–40 ms; E2E ≤50–80 ms.
5. Safety states BOOT/SEARCH/LOCK/CLOSE/ABORT/RTB/FTS — stubs only; policy = #safety.

## Ground EO + operator BDA (glue)

Contract: [`docs/icd/gcs_video_bda.md`](icd/gcs_video_bda.md) · notes: [`docs/notes/GROUND_EO_CUE_AND_BDA.md`](notes/GROUND_EO_CUE_AND_BDA.md)

| Item | Value |
|------|-------|
| Nose EO | Orin GMSL/Argus · `cam_id=0` |
| Ground EO | **GCS laptop** USB/V4L2 · `/dev/video0` · 30 fps · `cam_id=2` |
| Config stub | `vision/configs/capture_ground_eo_usb.yaml` (Perception owns live ingest) |
| Mark destroyed | `TARGET_DESTROYED` (USER_1 **3**) → **RTB always** |
| Mark miss/reattack | `MISS/REATTACK` (USER_1 **4**) → ABORT / allow WORK |
| Bridge | `send_target_destroyed` / `send_miss` / `send_reattack` in `onboard/mavlink_bridge/` |
| Logs | `bda.jsonl`, optional `gcs_events.jsonl` |

V1: commanded miss + operator BDA only — no autonomous kill.
