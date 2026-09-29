# Onboard TEST mode — pre-Engage checkout (PM 2026-09-18)

**User intent:** First flights with the stack on the airframe need a named **TEST** profile: get the system used to how the aircraft turns / speeds in different axes, and run **safety protocols in the first few seconds**, so later intercept is more accurate. **Not** full CLOSE/kill yet.

## Place in mission

```
MANUAL climb/position
  → TEST (this profile) — seconds-scale checkout
  → ENGAGED (CH7) — SEARCH / Lock test_drone
  → CLOSE → LAND/RTL
```

TEST is **between** manual and Engage. Abort (ELRS) always wins in every phase.

## Goals (first few seconds after TEST arm)

1. **Control familiarity / authority sample** — small, bounded attitude/rate or heading/altitude nudges so Orin/Nav learn or verify airframe response (flaps/elevons turn + speed) before prosecute.
2. **Safety protocols smoke** — prove abort, failsafe, and gate wires before SEARCH/CLOSE.
3. **Sensors alive** — EO frames, TrackMsg path dry, TELEM2 HB, no CLOSE.

## Safety protocols (must PASS before Engage allowed)

| ID | Check | Pass |
|----|-------|------|
| T-EA-1 | ELRS stick reclaim / abort | Immediate manual reclaim |
| T-EA-2 | Failsafe / RTL or hold on link loss (as Safety stamps) | Observed within policy time |
| T-HB | Orin↔Cube companion heartbeat | FS companion path armed |
| T-EO | Camera streaming + timestamp soft OK | Frames >0 Hz |
| T-AUTH | Control nudge budget | Rate/angle limits; no open-loop CLOSE |
| T-ABORT | Abort kills TEST immediately | Mode → MANUAL |

**Gate:** TEST PASS (TEST card) **required** before CH7 Engage is software-armed for CLOSE. Soft-kill/CONTACT still behind EA drills on fly_defeat card.

## Control sample (Nav / airframe) — bounded

- Duration: **~3–15 s** (TEST owns exact window)
- Axes: yaw/roll or elevon turn sample + optional pitch/speed sample matching rear-flap airframe
- Amplitude: **small** (Safety/TEST limits) — not aggressive chase
- Output: log response (IMU / RC / GUIDED echo) for later CLOSE gain confidence
- **Forbidden in TEST:** Lock→CLOSE, soft-kill score, ram

## Mode machine delta

Add `MissionPhase::kTest` (or Integration name) between MANUAL and ENGAGED:

- Enter: dedicated RC CH or GCS **TEST** button (not CH7 Engage)
- Exit PASS → allow Engage arm
- Exit FAIL / Abort → MANUAL; Engage disarmed

## Ownership

| Channel | OWN |
|---------|-----|
| test | TEST card + timing + go/no-go |
| safety | Protocol list + abort supremacy |
| Navigation | Bounded authority sample + logs |
| airframe-iface | Flap/elevon map for sample |
| Integration | Mode wire + CH map (TEST ≠ CH7) |
| Tracking | Idle / no Lock in TEST |
| System stack | GCS TEST state on rail |
| perception | N/A (no Lock) |
| hardware | No new buys; sensors already in min stack |
| comms | HB / RFD telem during TEST |

## Explicit

- TEST ≠ Engage ≠ CLOSE  
- Vercel/Supabase still out of C2  
- Mass A unchanged  
