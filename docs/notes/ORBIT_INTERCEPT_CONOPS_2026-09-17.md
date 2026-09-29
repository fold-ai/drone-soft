# Orbit → climb → match → Lock → kill CONOPS (freeze 2026-09-17)

Status: **PM freeze draft** — implement in Nav / Tracking / GCS; TEST gate before live air.

## Threat picture (operator-provided)
- Target UAV **orbits / loiters** in one geographic area (not a long straight transit).
- Operator supplies **approximate target lat/lon + altitude** (manual cue; not full autonomous search from Ground EO alone in this profile).

## Interceptor sequence (mandatory order)
1. **Takeoff / climb first** — gain altitude **before** lateral reposition toward the target orbit. Do not chase laterally at low altitude.
2. **Ingress / station** — after safe altitude, fly to a position relative to the cued orbit (stand-off / attack geometry TBD by Nav; default: outside orbit radius, nose-on or lead cone).
3. **Match energy** — accelerate to **approximately target speed** and align **heading ≈ target track / attack heading**.
4. **Acquire / Lock** — Nose EO (or onboard Orin hot path in flight) acquires the target; operator may **click-to-lock** the pointed target in GCS demo; production Lock is class-gated.
5. **Prosecute** — after valid **Lock ⇒ kill** (see `LOCK_EQUALS_KILL.md`): close and ram / commanded intercept. ABORT/FTS remain available. No “mark destroyed then decide” as the kill path.

## Manual cue interface (GCS / C2)
- Operator enters or confirms: `target_lat`, `target_lon`, `target_alt_m` (± tolerances).
- Optional: orbit radius / clock position if known.
- Mission profile id: `ORBIT_INTERCEPT` (distinct from `TEST_GEOMETRY` ~100 km/h miss and from `AIRFRAME_JET` until G3).

## Altitude-first rationale
- Clear terrain / own-ship safety.
- Better Nose EO look-down / look-across geometry once level at altitude.
- Avoids premature lateral commitment before energy match.

## Channel ownership
| Channel | Owns |
|---------|------|
| Navigation | Climb-first trajectory, orbit station, speed/heading match, attack geometry |
| Tracking | Track handoff from cue → EO lock; coast if EO blanks |
| perception | Class-gated Lock; click-to-lock demo vs production |
| Integration | Mode machine: BOOT→…→SEARCH→LOCK→CLOSE; cue ingestion |
| comms | Thin C2 for cue + Lock; video not on kill path |
| safety / test | Climb gate, geo-fence, ABORT/FTS, G1–G3 for this profile |
| airframe-iface / hardware | ArduPilot modes, payload limits, TELEM failsafe unchanged |
| System stack | Mission profile enum + GCS fields for manual cue |

## Explicit non-goals (this freeze)
- No autonomous wide-area search without cue.
- No buy of carrier until Integration PPS/trigger PASS (existing gate).
- Demo GoPro COCO lock ≠ production Shahed class Lock.

## Open items for channels (reply with OWNED + blockers)
1. Default stand-off distance / orbit entry geometry at 700–800 km/h class vs TEST_GEOMETRY.
2. Climb altitude rule: `max(target_alt + Δh, min_safe_alt)` — propose Δh.
3. Cue uncertainty (±m / ±alt) → search box size before Lock.
4. When click-to-lock is allowed vs AUTO detector (GCS SEMI/MANUAL).
