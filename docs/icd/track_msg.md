# TrackMsg ICD (Tracking → Navigation)

**Status:** V1 sketch (locked fields for measurement epoch)  
**Owner:** Tracking system + Navigation  
**Wire:** `interfaces/ros2_msgs/Track.msg`, `interfaces/include/actprove/track_msg.hpp`

## Concept

A track is an association of DetectionMsg boxes over time in the **PPS time domain**.  
**`measurement_epoch`** of any track update = PPS time of the frames that formed the update  
(mid-exposure `t_pps` from DetectionMsg). Soft host `now()` is **forbidden**.

**Coast updates:** keep the epoch of the **last real measurement** (do not advance to publish time).

## Fields (V1)

| Field | Type | Notes |
|-------|------|-------|
| `track_id` | uint32 | Stable while coasting |
| **`measurement_epoch`** | float64 | **REQUIRED** — PPS domain; primary name |
| `t_pps` | float64 | Historical alias; **must equal** `measurement_epoch` |
| `t_publish_pps` | float64 | Debug / latency probe only — never for guidance |
| `state` | enum | TENTATIVE / CONFIRMED / COAST / LOST |
| `cam_id` | uint8 | Primary modality for last update (EO=0, THERMAL=1) |
| `u, v` | float | Image-plane center (px) |
| `bbox_w, bbox_h` | float | Last box size |
| `range_est_m` | float | Optional V1; NaN if unknown |
| `bearing_ned_rad` | float[2] | Az/el in NED if available |
| `lock_quality` | float | 0..1 gate for SEARCH→LOCK |
| `class_id` | uint32 | From DetectionMsg |
| `age_updates` | uint32 | |
| `time_since_meas_s` | float | Age since last real measurement |

## Lock gate (safety)

- SEARCH→LOCK requires stable track + box (see V1_PLAN §6).
- Lost-box coast ≤ `T_coast` then return SEARCH.
- No new-target hunt without engagement corridor.
- CLOSE = heading / commanded miss only — **no terminal stick/kill**.

## Latency

E2E `measurement_epoch` → track publish: **≤ 50–80 ms** (see `latency_budget.md`).

## Handoff fields (Ground EO)

| Field | Notes |
|-------|-------|
| `handoff_phase` | Idle / GroundSeeded / AwaitNose / Onboard / PostMiss |
| `cue_source` | CamId of seed / owner |
| `onboard_owns` | true ⇒ CLOSE may use track |
| `operator_lock_req` | pulse from Ground / manual Lock |

See `docs/icd/ground_eo_handoff.md`.
