# Navigation audit + virtual tests

**Channel:** Navigation  
**Date:** 2026-09-16  
**Scope:** `/workspace/actprove-drone/navigation/` + related ICDs  
**Language:** English  
**Trigger:** PM user-directed full Nav audit  

---

## 1. Executive result

| Item | Result |
|------|--------|
| `navigation_smoke` | **PASS** (exit 0) |
| Soft-land profile (TEST_RECOVER) | **PASS** in smoke (bleed → descend → disarm) |
| BDA `TARGET_DESTROYED` → RTB | **PASS** (always RTB; does not fight LAND_SOFT) |
| Soft-land only auto post-CLOSE miss | **PASS** (`land_soft_active` without destroyed mark) |
| Ground cue bearing-only heading | **PASS** (accept with image box; reject map-pin) |
| MISS/REATTACK re-cue | **PASS** (clear CLOSE, accept cue heading) |
| Build | `libnavigation.a` + `navigation_smoke` link OK |
| Overall Nav V1 scaffold | **CONDITIONAL PASS** — logic/ICD sound; HW sync (DRDY→HTE) still HOLD |

---

## 2. Virtual tests run

```bash
cmake --build navigation/build -j
ctest --test-dir navigation/build --output-on-failure
./navigation/build/navigation_smoke
# → navigation_smoke OK (miss + soft-land + cue/BDA/RTB)
```

### Coverage matrix (smoke)

| Check | Asserted |
|-------|----------|
| Commanded miss ∈ [15, 30] m; miss=0 throws | Yes |
| Heading guidance → valid vel+yaw setpoint | Yes |
| ADIS free-run+DRDY OK; cam-FSYNC / soft-stamp forbidden | Yes |
| Soft-land inactive when flag false | Yes |
| Soft-land bleed caps ≤ ~18 m/s | Yes |
| Soft-land descend `vz` ∈ [1, 2] m/s NED | Yes |
| Soft-land WoW → disarm request | Yes |
| Ground cue bearing-only + image box → accept | Yes |
| Map-pin (no image box) → reject | Yes |
| Cue heading bias → valid setpoint | Yes |
| `TARGET_DESTROYED` → RTB | Yes |
| `TARGET_DESTROYED` + `land_soft_active` → still RTB | Yes |
| `land_soft_active` alone → SoftLand | Yes |
| `MISS` + cue → CueHeading + `accept_recue` | Yes |
| RTB stub setpoint toward home | Yes |

**Gaps (not yet virtual-tested):** closed-loop PX4 SITL, latency budget on live TELEM2, real ADIS DRDY HTE path, wind-aware soft-land, energy/jet-idle soft-land tuning.

---

## 3. Package inventory

| Path | Role | Status |
|------|------|--------|
| `track_input.hpp` | Track → Nav ICD (`t_pps`) | OK |
| `miss_geometry.hpp` | 15–30 m commanded miss | OK |
| `heading_guidance.*` | CLOSE LOS → vel+yaw | Stub OK |
| `mavlink_setpoint.hpp` | Msg 84 vel+yaw wire | OK |
| `adis_sync.hpp` | IMU sync contract | OK (policy) |
| `soft_land_profile.*` | TEST_RECOVER land | OK |
| `ground_cue.hpp` | Cue accept (bearing-only) | OK |
| `cue_heading.*` | Cue → heading bias | OK |
| `bda_exit.hpp` | Exit selector | OK |
| `rtb_profile.*` | RTB home stub | OK |
| `docs/guidance_icd.md` | Guidance ICD | OK |
| `docs/soft_land.md` | Soft-land ICD | OK |
| `docs/ground_cue_and_bda.md` | Cue + BDA ICD | OK |
| `configs/guidance_v1.yaml` | Tunables | OK |

---

## 4. Exit-path policy (verified)

Priority (no new `MISSION_STATE`; BOOT..FTS 0–6 frozen):

1. **`TARGET_DESTROYED`** → **RTB** always (clean from CLOSE/ABORT; never compete with LAND_SOFT).  
2. **`land_soft_active`** (auto TEST_RECOVER after CLOSE miss) → **SoftLand**.  
3. **SM RTB** → RTB.  
4. **`MISS` / `REATTACK`** → clear CLOSE; CueHeading if cue else idle.  
5. CLOSE + usable nose track → commanded-miss guidance.  
6. Cue usable → heading bias.  
7. Else idle (no free-hunt).

---

## 5. Ground cue acceptance (verified)

- **Accepted:** ground/operator cue with **image box**, `cue_quality ≥ 0.3`, finite azimuth; **range may be NaN** (bearing-only).  
- **Rejected:** map-pin only (`has_image_box=false`), unknown source, low quality.  
- Nose track handoff supersedes ground cue for CLOSE (documented; integration with Tracking wire still open).

---

## 6. N250 jet constraints (Navigation)

Airframe is a **small turbojet-class** interceptor (N250-class / turbine island), not a multirotor. Nav must not assume prop-UAV air-data behavior.

### 6.1 Vibration / IMU

| Constraint | Nav implication |
|------------|-----------------|
| High broadband vibration from turbine | ADIS16470-class on **isolated EO+IMU bracket** — **not** turbine mount (airframe PASS) |
| Soft-mounted IMU still needs rigid EO↔IMU lever arm | Extrinsics / lever-arm cal is first-class; do not “float” IMU relative to EO |
| Vibration aliases into gyro bias | Prefer free-run + DRDY HW-ts; reject soft SPI stamps; keep sample rate ≥200–500 Hz |
| Cam FSYNC must not clock IMU | Locked in `adis_sync.hpp` |

### 6.2 Air data / pitot (no prop-wash assumptions)

| Constraint | Nav implication |
|------------|-----------------|
| **No propeller wash** over a nose pitot | Do **not** copy multicopter “prop-induced IAS” bias models |
| Jet intake / plume can corrupt a poorly placed pitot | Pitot placement is airframe-owned; Nav treats IAS as **optional / quality-flagged** |
| Pitot currently **HOLD** in BOM | Soft-land / RTB must degrade gracefully on **GNSS groundspeed + attitude** if IAS missing or invalid |
| Soft-land `airspeed_cap_mps` | Treat as **indicated or groundspeed proxy**; label source in logs (`pitot` \| `gps_groundspeed` \| `na`) — see test schemas |
| Baro required | Vertical channel for descend / AGL proxy when ranging absent |

### 6.3 Guidance / energy

| Constraint | Nav implication |
|------------|-----------------|
| Jet spool / thrust lag ≠ prop instantaneous thrust | Soft-land bleed phase must be **time + speed** gated, not assume instant deceleration |
| High IAS CLOSE passes | Miss geometry 15–30 m; no stick/kill; operator BDA gates RTB vs re-cue |
| Recoverable TEST_RECOVER soft-land | Bleed → descend 1–2 m/s → disarm; not a jet flameout procedure — range-test profile only |

### 6.4 Nav acceptance criteria (N250)

- [ ] IMU not on turbine hardpoints (mechanical stamp).  
- [ ] Soft-land / RTB run with **IAS unavailable** (GPS GS fallback) without asserting false airspeed.  
- [ ] Logs tag airspeed source.  
- [ ] No guidance law assumes propwash-calibrated pitot.  
- [ ] Vibration: DRDY HW-ts path PASS before Nav Integration PASS.

---

## 7. Open blockers (Nav)

| ID | Blocker | Blocks |
|----|---------|--------|
| N1 | Forecr DRDY→HTE GPIO unnamed (FAE) | Nav Integration PASS / carrier PO |
| N2 | PPS/FSYNC pin names on Forecr | Time domain for cue/track fusion |
| N3 | Pitot HOLD / airspeed source ICD incomplete | Soft-land speed gate fidelity on jet |
| N4 | No PX4 SITL closed-loop yet | Flight confidence beyond unit smoke |
| N5 | Tracking↔Nav wire for nose handoff not live | End-to-end cue→CLOSE |

---

## 8. Doc fixes made during audit

- `navigation/README.md`: corrected stale line that said TARGET_DESTROYED could defer to soft-land; now matches `bda_exit.hpp` (destroyed → always RTB).

---

## 9. Recommendation

- **Keep** Nav scaffold as V1 baseline.  
- **Next:** (1) wire `select_nav_exit` + profiles into mavlink_bridge, (2) SITL virtual mission CLOSE→BDA→RTB and CLOSE→TEST_RECOVER→soft-land, (3) close Forecr FAE for DRDY HTE, (4) add airspeed-source enum to soft-land input + smoke case with IAS=`nan`.

**Audit stamp:** Navigation — CONDITIONAL PASS (software/ICD); HW sync HOLD.
