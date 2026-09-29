# Manual climb → Engage button → auto defeat test drone → auto-land (PM 2026-09-18)

## User freezes
1. **Mass:** whole *avionics/sense/compute island* ≤ **1.5 lb (~680 g)** — tighter than prior ≤2 kg payload stamp; channels must re-budget.
2. **Airframe note:** rear wing flaps (elevons/ailerons) = turn + speed authority (airframe-iface).
3. **Pilot flow:**
   - Manual RC climb / position
   - Dedicated **Engage** RC switch/button → handoff to onboard detect→track→CLOSE on **test_drone**
   - After defeat (contact or soft-kill ≤15 m): pilot or auto **LAND/RTL** (existing soft-land)
4. **Speed:** slow platform first, still “high” vs ground — **not** 800 km/h jet; prepare FoV/exposure/track for tens of m/s class.
5. **Cloud:** Vercel/Supabase = ops/training/metrics only — **never** flight C2. Flight = onboard Orin + ELRS + RFD.

## Engage architecture (controller ↔ drone)
```
ELRS TX (pilot)
  ├─ sticks → Cube RCIN (manual)
  └─ Engage CH (e.g. CH7 high) → Cube → Orin (RC_CHANNELS / custom) OR Orin reads RC via MAVLink
       → arms SEARCH/Lock on test_drone → GUIDED CLOSE
RFD900 TELEM1 ↔ GCS laptop: telem + ABORT + cue (optional)
Orin TELEM2 ↔ Cube: TrackMsg / GUIDED setpoints / LAND
Vercel/Supabase: post-flight logs / training only
```

**Engage button:** one spare RC channel, edge-triggered, safety-gated (arm only after EA drills; abort = ELRS).

## Camera — what we look for (this profile)
| Characteristic | Target (test / tens of m/s) | Why |
|----------------|----------------------------|-----|
| Shutter | **Global shutter** preferred | Less smear when turning / closing |
| Exposure | ≤0.5–2 ms class (as fast as light allows) | Motion blur |
| Frame rate | ≥60 Hz (30 Hz absolute floor) | Track hold |
| HFOV | **~20–40°** acquisition / ~10–15° if already cued | Wider than Cam A 10° for manual-setup hunt |
| Interface | Prefer lightweight USB3 / CSI for interim; GMSL2 if mass allows | 1.5 lb budget |
| Mass | Aim **≤80–150 g** cam+lens | Island ≤680 g total |
| Trigger / sync | Nice-to-have; soft ts OK this gate ≠ Lock PASS | 2-week descope |
| Resolution | ≥720p; 1080p OK if FPS held | Detect |

**Prior Cam A Basler 168fps+40mm** stays long-term Lock optic; may **blow 1.5 lb island** with Orin+carrier+RFD. For this variant: pick lighter interim EO that fits budget; keep Basler as upgrade path.

## Mass budget sketch (must validate)
| Block | Budget hint |
|-------|-------------|
| Orin module + light carrier + cooler | ~200–350 g |
| Cam+lens interim | ~80–150 g |
| RFD900x + ants (share airframe) | ~50–80 g |
| Cables/mount | ~50 g |
| Margin | rest to 680 g |
Cube+GPS+ELRS RX often on airframe side — **clarify with user/hw** if 1.5 lb includes FC or only companion island.

## Software must
- RC Engage channel → mission ARM_SEARCH
- class Lock `test_drone` only
- GUIDED CLOSE; soft-kill 15 m or contact
- LAND/RTL after terminal
- No Vercel in control loop

## Channel ownership
See fan-out. Reply OWNED + mass sheet + Engage CH map.
