# time_sync — PPS master / HW timestamp distribution

**Owner:** System stack (+ Navigation for IMU domain)  
**ICD:** `docs/icd/time_sync.md`, `docs/icd/latency_budget.md`

## Purpose

Ingest GNSS PPS (or FC PPS) on Orin, discipline a shared `PpsClock`, and distribute
measurement epochs to EO/thermal FSYNC and IMU DRDY HW-ts. Soft `now()` stamps are
**forbidden** for cameras, IMU, DetectionMsg, and TrackMsg.

## Types

| Type | Role |
|------|------|
| `PpsClock` | Shared PPS-disciplined clock; `ready_for_cameras()` gate |
| `FrameTimestamp` | `measurement_epoch_ns` (mid-exposure) + optional publish time |
| `PpsIngest` | Edge wait loop (GPIO/PPS/HTE TBD) |

## Build / run

```bash
cmake -S onboard -B onboard/build && cmake --build onboard/build -j
# Link consumers against target `time_sync`
```

## Start order

Bring-up **must** start `time_sync` before logging cameras. See `onboard/bringup/`.

## TODOs (kernel / GPIO)

- [ ] Map Forecr PPS GPIO → `/dev/pps0` or HTE channel (FAE open)
- [ ] Generate cam FSYNC 30/60 Hz from same domain
- [ ] ADIS DRDY → HTE (currently FAIL on Forecr until FAE)
