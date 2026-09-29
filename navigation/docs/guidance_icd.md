# Guidance ICD (Navigation V1)

**Owner:** Navigation  
**Consumers:** Tracking (in), onboard mavlink_bridge (out), Safety (arm/CLOSE gate)

## Inputs

| Source | Type | Notes |
|--------|------|-------|
| Tracking | `TrackMsg` | `t_pps` measurement epoch; LOCK quality ≥0.5 for CLOSE |
| Own-ship | `OwnshipState` | PPS-domain fused state (not GPS_RAW for guidance) |
| Config | `configs/guidance_v1.yaml` | Miss 15–30 m; setpoint 10–20 Hz |

## Outputs

| Sink | Type | Notes |
|------|------|-------|
| mavlink_bridge | `VelYawSetpoint` → `SET_POSITION_TARGET_LOCAL_NED` (84) | vel+yaw only; `valid=false` ⇒ do not send |
| Safety / logger | miss distance, side, guidance epoch | black-box |

## Commanded miss (non-negotiable)

- Lateral miss **15–30 m** (G3). Default 20 m.
- **Never** command 0 m / impact / stick / kill.
- Side fixed per sortie card (`left`/`right`).

## ADIS / time

- Free-run + DRDY HW-ts on PPS clock.
- Cam FSYNC must not drive ADIS SYNC.
- Soft SPI stamp = Nav REJECT.

## Safety coupling

- Publish setpoints only in **CLOSE**.
- Lost track → coast ≤ `coast_lost_track_s` then Safety returns SEARCH.
- Lost-link → RTB (not FTS). FTS never from Nav.
