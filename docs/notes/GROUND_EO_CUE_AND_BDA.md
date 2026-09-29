# Ground EO cue + operator BDA → RTB (range test, no radar)

**PM freeze draft 2026-09-16** — V1 range test when interceptor and target share geography and **no radar** is available.

## Problem
- No radar in V1.
- Target may be outside interceptor nose FOV at start (same range / trail / pad).
- High-speed pass often **misses** a small surrogate on first CLOSE — need re-attack, not assume kill.

## Test architecture

```
[Ground EO camera] --USB/capture--> [GCS laptop] --RFD900 C2--> [Interceptor FC/Orin]
       ^ operator aims + Lock                         near-field video optional
```

1. **Ground EO** (manual pan/tilt or operator-held) feeds the **same GCS** as V1 demo (not a second aiming camera on the airframe).
2. Operator aims ground camera at target → **Manual Lock** (image box) on ground EO feed.
3. GCS sends **cue** to interceptor: bearing / approximate LOS + `operator_lock_request` / WORK as available.
4. Interceptor launches / turns; onboard nose EO acquires when target enters FOV → track handoff (ground seed → onboard track).
5. **CLOSE** = commanded miss / intercept attempt (V1: not kill stick).
6. Operator marks **BDA** in GCS:
   - `TARGET_DESTROYED` → clear engagement → **RTB** (return to base / recovery plan).
   - `MISS` / `REATTACK` → RTB or orbit for second pass (range SOP); do **not** auto-declare kill.
7. If `TEST_RECOVER=1` and soft surrogate + recoverable interceptor: prefer soft-land path after successful BDA **or** after abort — see existing LAND_SOFT freeze. For high-speed miss loops, default **RTB** after BDA mark.

## Why RTB after mark
At high IAS a small target is easy to miss; the airframe should **not** keep hunting forever. Operator BDA is the gate: destroyed → home; miss → re-cue / second WORK per test card.

## New GCS controls (English UI)
- Source toggle: **Nose EO** | **Ground EO** (USB demo / capture).
- Button: **Mark destroyed** → `TARGET_DESTROYED` → RTB.
- Button: **Mark miss / reattack** → clear CLOSE, allow new WORK or RTB.

## Owners
| Piece | Channel |
|-------|---------|
| Ground EO ingest (V4L2/USB) + GCS source toggle | perception + System stack + GCS |
| Cue / handoff ground box → onboard track | Tracking + Navigation |
| `TARGET_DESTROYED` / `MISS` cmds + SM → RTB | safety + comms |
| Test card G-RANGE-NR-V1: `test/g_range_no_radar/TEST_CARD.md` | test |
| Class training on Shahed-136 / Geran-2 / Gerbera inbox | perception |

## Non-goals (V1)
- Radar.
- Autonomous kill declaration without operator BDA.
- Map-pin-only Lock.

## PM + SAFETY + COMMS freeze (2026-09-16)

Uplink via existing `MAV_CMD_USER_1` (31000) — **no custom dialect**. Stamped:

| Name | param1 | Effect |
|------|--------|--------|
| `TARGET_DESTROYED` | **3** | Clear engagement → **RTB always** (even if `TEST_RECOVER` armed). No LAND_SOFT on this path. |
| `MISS` / `REATTACK` | **4** | Clear CLOSE/ABORT → allow **re-WORK**. One wire value for both UX labels. |

**No param1=5.**

**LAND_SOFT** only on automatic post-CLOSE miss when `TEST_RECOVER=1` before WORK — not after BDA destroy.

Ground Lock = **image box** only. Cue: ground EO → onboard nose when in FOV.



---

## Perception handoff (appended)

Owner: Perception (`vision/`). Details: `vision/notes/GROUND_EO_MANUAL_LOCK.md`, `vision/notes/GROUND_LOCK_HANDOFF.md`.

- Ground EO is **GCS laptop USB/V4L2** (`/dev/video0`), not Orin nose GMSL.
- After Manual Lock on Ground EO image box, Perception publishes `DetectionMsg` with **`cam_id=2`** (`CamId::GroundEo`).
- Nose EO remains `cam_id=0`. THERMAL=`1` unused until V1.1.
- **No map-pin Lock** — image box only (Ground EO or nose EO).
- Manual designate conf=`1.0`; class gate `{shahed_136=0, geran_2=1, gerbera=2}`.
- Config: `vision/configs/capture_ground_eo_usb.yaml`.
