# Navigation — heading + commanded miss (V1)

**Owner:** Navigation  
**V1 job:** Consume a locked track → publish **heading / velocity+yaw** setpoints that fly a **commanded miss (15–30 m)**.  
**Hard forbid:** terminal stick, impact guidance, kill / PN-to-contact, combat-UAV envelopes.

Source of truth: repo `V1_PLAN.md` (§1 goal, §5 MAVLink, §6 CLOSE, §7 G3 miss 15–30 m).

## Layout

| Path | Role |
|------|------|
| `include/navigation/track_input.hpp` | Track → Nav ICD (`t_pps` epoch) |
| `include/navigation/miss_geometry.hpp` | Lateral miss 15–30 m (never 0) |
| `include/navigation/heading_guidance.hpp` | LOCK→CLOSE heading / LOS → setpoint |
| `include/navigation/mavlink_setpoint.hpp` | Fill `SET_POSITION_TARGET_LOCAL_NED` (vel+yaw) |
| `include/navigation/adis_sync.hpp` | ADIS16470 free-run + DRDY HW-ts contract |
| `include/navigation/soft_land_profile.hpp` | TEST_RECOVER soft-land (ABORT submode) |
| `src/heading_guidance.cpp` | Guidance stub (week-0) |
| `configs/guidance_v1.yaml` | Miss distance, rates, coast |
| `docs/guidance_icd.md` | Nav↔Tracking / Nav↔MAVLink contract |
| `tests/smoke_test.cpp` | Miss bounds + setpoint + ADIS policy |

## Hot path

```
TrackMsg (Tracking, t_pps)
  → HeadingGuidance::update()
  → MissGeometry::miss_offset_ned()   # 15–30 m
  → VelYawSetpoint / to_wire()
  → onboard mavlink_bridge → TELEM2 → PX4
```

Rate: **10–20 Hz** in `CLOSE` only.

## ADIS / time

- **Free-run + DRDY HW-timestamped** on PPS-disciplined Orin clock.
- Cam FSYNC must **never** drive ADIS `SYNC`.
- Soft SPI stamp = **REJECT** for Nav PASS.
- Forecr SPI0 @ 3.3 V ACCEPT; DRDY→HTE still HOLD (FAE).

## Build

```bash
cmake -S navigation -B navigation/build
cmake --build navigation/build -j
ctest --test-dir navigation/build --output-on-failure
```


## Soft-land (TEST_RECOVER)

When SAFETY sets ABORT + `ap_mission_sm_land_soft_active()` (TEST_RECOVER path after CLOSE miss):

1. Cap airspeed → bleed
2. Gentle descent **−1…−2 m/s** (NED `+vz`), wings level / hold heading
3. Disarm on WoW or timeout

Navigation owns the profile; SM only gates the mode. **No new MISSION_STATE enum.**


## Ground cue + BDA exits

See `docs/ground_cue_and_bda.md` (English ICD).

- **Bearing-only ground cue accepted** until nose track acquires (`ground_cue.hpp`, `cue_heading.hpp`).
- **TARGET_DESTROYED** → always RTB (`rtb_profile.hpp`); soft-land only on auto TEST_RECOVER post-CLOSE miss (`land_soft_active` without destroyed mark).
- **MISS / REATTACK** → clear CLOSE guidance, accept re-cue heading (`bda_exit.hpp`).
- No new MISSION_STATE; no autonomous kill declaration.
