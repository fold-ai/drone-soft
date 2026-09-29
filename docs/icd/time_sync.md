# Time Sync ICD

**Owner:** System stack + Navigation  
**Hard gate:** all sensors on one PPS / HW-trigger time domain.  
**Code:** `onboard/time_sync/` (`PpsClock`, `FrameTimestamp`, `PpsIngest`)

## Master

```
GNSS PPS (ZED-F9P class) ──► Orin / sync mezz SYNC_IN
                               ├─ cam FSYNC 30/60 Hz ──► EO + thermal
                               └─ IMU: free-run ADIS + DRDY HW-ts
                                  (or high-rate SYNC 200–2000 Hz)
                                  ✗ never ADIS SYNC from cam @ ≤30 Hz
```

## Rules

1. `Frame.t_pps` / `FrameTimestamp.measurement_epoch_ns` = **mid-exposure** latch, not dequeue time.
2. IMU samples stamped on DRDY edge in PPS-disciplined clock (target ≪1 ms jitter).
3. Soft host `now()` timestamps on SPI read / image dequeue = **REJECT**.
4. DetectionMsg / TrackMsg / guidance setpoints carry `t_pps` or **`measurement_epoch`** derived from it.
5. Coast track updates keep epoch of last real measurement.
6. **Cameras must not start** until `PpsClock::ready_for_cameras()` (bringup start-order).
7. Carrier buy blocked until Integration PASS: PPS in + FSYNC out + DRDY→HTE named.

## Forecr status (2026-09-16)

- SPI0: PASS · DRDY→HTE: **FAIL** (FAE) · PPS/FSYNC pins: unnamed
