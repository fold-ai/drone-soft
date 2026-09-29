# extrinsics_calib — EO↔IMU lever-arm / time-offset

**Process owner:** Navigation (+ hardware mount)  
**Path host:** System stack (`tools/extrinsics_calib/` per V1_PLAN §4)

## YAML schema

See `lever_arm.example.yaml`:

- `lever_arm_imu_m` — EO origin in IMU frame (m)
- `rpy_imu_to_eo_rad` — IMU→EO rotation
- `t_offset_s` — `t_eo = t_imu + t_offset_s` (PPS domain)

## Stub CLI

```bash
python3 tools/extrinsics_calib/extrinsics_calib.py
python3 tools/extrinsics_calib/extrinsics_calib.py path/to/lever_arm.yaml
```

Validates schema only. Full flash / chessboard / FSYNC estimation lands with Navigation.
