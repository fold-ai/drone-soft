# tracking/ — box → track → lock (+ Ground EO handoff)

**Owner:** Tracking system  
**Input:** `DetectionMsg` (nose EO) and/or `GroundCueMsg` (GCS Ground EO Lock)  
**Output:** `TrackMsg` → Navigation / Safety (`measurement_epoch = t_pps`)  
**Out of scope:** PWM, guidance setpoints, MAVLink TX, kill declaration

## V1 scope

- Associate boxes; confirm lock for SEARCH→LOCK.
- Coast on lost-box ≤ `T_coast` then SEARCH.
- **Ground EO → nose handoff** (no-radar range test): operator Lock on Ground EO seeds track; nose EO same-class acquire takes ownership for CLOSE.
- **Re-attack:** after BDA `MISS`, re-seed from Ground EO without full BOOT.
- Thermal / dual-GMSL fusion: **V1.1**.
- ROI hints to Perception: V1.1.

## Layout

```
tracking/
├── README.md
├── CMakeLists.txt
├── include/tracking/
│   ├── detection_msg.hpp
│   ├── track_msg.hpp
│   ├── ground_cue_msg.hpp
│   └── tracker.hpp
└── src/
    ├── tracker.cpp
    └── smoke_main.cpp
```

Shared wire: `interfaces/include/actprove/`.  
ICD: `docs/icd/track_msg.md`, `docs/icd/ground_eo_handoff.md`.  
Spec: `docs/notes/GROUND_EO_CUE_AND_BDA.md`.

## Handoff phases

| Phase | Meaning |
|-------|---------|
| Idle | No cue |
| GroundSeeded / AwaitNose | Ground Lock active; waiting nose FOV |
| Onboard | Nose owns track; `onboard_owns` → CLOSE |
| PostMiss | BDA miss; Ground re-seed OK |

## Build (host with toolchain)

```bash
cmake -S tracking -B tracking/build && cmake --build tracking/build -j
./tracking/build/tracking_smoke
```
