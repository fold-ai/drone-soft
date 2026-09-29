# logging — NVMe black-box

**Owner:** System stack  
**ICD / gates:** `docs/bringup/logger_smoke.md`, `test/g1_g3/LOG_LAYOUT.md`  
**Capacity:** size NVMe for **≥1–2 h** continuous V1 rates.

## Streams

See `schema/blackbox_streams.json`:

| Stream | Notes |
|--------|-------|
| `camera_ts` | PPS mid-exposure; `cam_id` NoseEO=0 / THERMAL=1 / GroundEo=2 |
| `tracks` | `measurement_epoch_ns` required (coast keeps last real epoch) |
| `ownship` | Airspeed / heading / alt |
| `mavlink_tx` | SET_POSITION_TARGET_LOCAL_NED setpoints |
| `mission_state` | BOOT…FTS |
| `bda` | Operator BDA: `{publish_time_ns, cmd, mission_state, video_source}` |
| `gcs_events` | Optional GCS events (e.g. `video_source_changed`) |
| `boxes` | Optional DetectionMsg mirror (G1) |

`cmd` UX labels: `TARGET_DESTROYED` | `MISS` | `REATTACK`. Wire string for miss/reattack into safety_gates remains `MISS/REATTACK`.  
`video_source`: `nose` | `ground` (GCS toggle; Ground EO = laptop USB/V4L2, not Orin nose).

## Build / smoke

```bash
cmake -S onboard -B onboard/build && cmake --build onboard/build -j --target logger_smoke
./onboard/build/logger_smoke --out /tmp/actprove_blackbox --seconds 2
```

Orin production root: `/data/blackbox`.
