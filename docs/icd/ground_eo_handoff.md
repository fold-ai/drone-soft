# Ground EO → onboard track handoff (Tracking)

**Status:** V1 implemented in `tracking/`  
**Spec:** `docs/notes/GROUND_EO_CUE_AND_BDA.md`  
**Owner:** Tracking (+ Navigation for bearing cue, Safety for BDA→RTB)

## Flow

1. GCS **Ground EO** Manual Lock → `GroundCueMsg` (image box required).
2. Tracking `seedFromGround` → `HandoffPhase::GroundSeeded` / `AwaitNose`.
   - `operator_lock_req=true`, `onboard_owns=false` (CLOSE not yet).
   - Optional `bearing_ned_rad` for Nav SEARCH bias.
3. Nose EO `DetectionMsg` (`cam_id=EO`) with **same `class_id`** → handoff.
   - `HandoffPhase::Onboard`, `onboard_owns=true` → CLOSE eligible.
4. Operator BDA:
   - `MISS` → `PostMiss`; re-seed via Ground EO **without BOOT**.
   - `TARGET_DESTROYED` → clear track; re-seed blocked until reset; Safety→RTB.

## Wire

| Message | Path |
|---------|------|
| `GroundCueMsg` | `interfaces/include/actprove/ground_cue_msg.hpp` |
| `DetectionMsg` + `CamId::kGroundEo=2` | detection ICD |
| `TrackMsg.handoff_phase` / `onboard_owns` | track ICD |

## Rules

- No map-pin-only Lock (box w/h must be > 1 px).
- No PWM from Tracking.
- V1 sensing for onboard acquire = nose EO only (thermal V1.1).
