# Gates G1–G3

| Gate | Phase | Pass criteria |
|------|-------|---------------|
| **G1 Bench** | see (detect) | Detector ≥60 Hz; frame→box <40 ms p95; PPS on frames **and** IMU |
| **G2 Iron bird** | see→lock→heading | Box path → MAVLink heading in FC log; SAFETY WORK-only entry SEARCH; states logged BOOT…FTS; FTS scripted = idle+surfaces+fuel-cut (no explosive) |
| **G3 Flight** | see→lock→heading→commanded miss @ ~100 km/h | Coop target ~100 km/h; own-ship 150–200 km/h max; lock 8–15 s; miss 15–30 m; **10** sorties |

## Evidence folders

```
G1_YYYYMMDD_HHMMSS_<id>/
G2_.../
G3_.../
```

Required streams: camera_ts, boxes, frame→box_ms, detector Hz, imu_ts+PPS, lock_state, MAVLink setpoints, FC log excerpt.

**FTS holder: unnamed — blocks G2/G3 schedule until named.**

Cross-cutting: lost-link → RTB; lost-track → SEARCH; SEARCH timeout → ABORT. No stick/kill. No Shahed-speed in V1.

## Related range card (no radar)

Ops procedure (not an envelope unlock): [`../g_range_no_radar/TEST_CARD.md`](../g_range_no_radar/TEST_CARD.md) — ground EO cue → Manual Lock → WORK → CLOSE → operator BDA (`TARGET_DESTROYED`→RTB or `MISS`/`REATTACK`→re-WORK). Spec: `docs/notes/GROUND_EO_CUE_AND_BDA.md`.
