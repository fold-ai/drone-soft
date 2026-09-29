# Camera → control + test link (PM stamp 2026-09-18)

## What the Mac GoPro demo is (and is not)
- **Is:** operator UI smoke — click/Capture, DEMO accompany, EST RANGE, OrbitCue SEARCH bias.
- **Is not:** flight guidance. DEMO track / EST / COCO never fly the jet. See `docs/integration/DEMO_TRACK_VS_LOCK.md`.

## How the real camera steers the interceptor
```
Nose Cam A (Basler GS, onboard Orin)
  → class-gated detect (shahed_136 / geran_2 / gerbera ONLY)
  → Tracking hold/coast (PPS epoch)
  → Lock (class PASS)  ⇒  prosecute / kill path
  → Nav: climb-first → station → match speed/heading → CLOSE (GUIDED setpoints)
C2 thin: RFD900 MAVLink (cue, WORK, ABORT, telem) — not video, not pixel lock from GCS
Near-field video (FIM-2410): operator SA only — off kill path
```

### Reject people / ground clutter
1. **Class gate** — Lock only on trained UAV classes; person/car/COCO ≠ Lock.
2. **Mission geometry** — ORBIT_INTERCEPT cue at altitude; climb gate before lateral; geo-fence.
3. **FoV** — Cam A ~10° tele; look-up / look-across at target alt, not ground crowd.
4. **DEMO labels** — Mac GoPro may see bottles/people; must stay DEMO.

### Speed / altitude / turns adaptation
- **Tracking:** high-speed hold (inflate jump gate on closing; coast ≤0.5 s; re-acquire).
- **Nav:** match `target_speed_est`; Δh climb; stand-off; lead cone; GUIDED vel+yaw.
- **Airframe:** Cube Orange GUIDED/AUTO; ABORT/RTB/FTS unchanged.
- EST RANGE = soft cue only — **not** guidance truth.

## How we talk to the aircraft (test vs demo)
Today (Mac): camera + local GCS file only — **no RF to airframe**.

### Minimum test link architecture
| Link | Role | Band / kit |
|------|------|------------|
| **C2** | Cue, modes, telem, ABORT | **RFD900x pair** — air TELEM1 ↔ ground USB/serial into GCS |
| **RC override** | Pilot kill-switch / manual | **ELRS/CRSF** (Safety HOLD for buy until stamped — still required for range ops) |
| **Near-field video** | Optional SA | **IFLY FIM-2410** pair — ≠ RFD, ≠ Lock path |
| **GCS** | Operator | Laptop + ACTPROVE GCS / Mission Planner bridge |

Ground “пульт” for test = **(1) RFD900 ground modem + GCS laptop** for command path + **(2) ELRS TX** for hard override — not the GoPro.

### Profiles
- `TEST_GEOMETRY` — ~100 km/h commanded miss (safer bring-up).
- `ORBIT_INTERCEPT` — climb-first + Lock⇒kill doctrine (gated).
- `AIRFRAME_JET` — 700–800 km/h — NO-GO until G3.

## Channel work (this stamp)
See fan-out messages — perception class reject people; Tracking/Nav speed adapt; comms/test-link bench; safety RC; hardware BOM refresh; Integration mode machine; System stack GCS↔RFD bridge.
