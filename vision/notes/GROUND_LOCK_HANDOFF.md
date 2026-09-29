# Ground Lock handoff (Perception → Tracking) — no map pin

Cross-link: `docs/notes/GROUND_EO_CUE_AND_BDA.md` (PM cue/BDA/RTB). This note is the **Perception handoff** after operator Manual Lock on Ground EO.

## Explicit rules
1. **Do NOT lock from map pin** — Lock only from an **image box** on Ground EO or nose EO.
2. After Manual Lock on Ground EO feed, Perception publishes `DetectionMsg` (or track seed) to Tracking.
3. Manual designate: operator click/drag box **OR** confirm detector proposal → **same** `DetectionMsg` wire.
4. Class gate (range test): Lock only if `class_id ∈ {0=shahed_136, 1=geran_2, 2=gerbera}` (or operator-forced among those).

## DetectionMsg fields Tracking needs

| Field | Ground EO Manual Lock | Notes |
|-------|----------------------|-------|
| `t_pps` | mid-exposure / designate time (PPS domain if available; GCS may use host clock mapped) | Correlation key |
| `seq` | capture sequence | |
| `cam_id` | **2** (`CamId::GroundEo`) | Distinguishes from nose EO=0 |
| `src_w` / `src_h` | source frame size | Box in full-frame pixels |
| `n` | ≥1 | |
| `boxes[].x,y,w,h` | image box, top-left origin, full-sensor pixels | **Not** map lat/lon |
| `boxes[].conf` | **1.0** for manual designate; detector conf otherwise | |
| `boxes[].class_id` | gated class | 0/1/2 only for range test |

## Flow
```
[Ground EO USB] → GCS display → operator Manual Lock (image box)
       → Perception publishes DetectionMsg (cam_id=2)
       → Tracking seeds / cues from box
       → Navigation cue heading (see GROUND_EO_CUE_AND_BDA)
       → Nose EO acquires when target in FOV (cam_id=0) → onboard track handoff
```

## Non-goals
- Map-pin-only Lock
- Publishing Ground EO as `cam_id=0`
- Radar
