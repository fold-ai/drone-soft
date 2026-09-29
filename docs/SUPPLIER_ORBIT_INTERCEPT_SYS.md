# System stack ACK — ORBIT_INTERCEPT CONOPS

**Stamp:** 2026-09-17 CT  
**Owner:** System stack  
**Authority:** `docs/notes/ORBIT_INTERCEPT_CONOPS_2026-09-17.md`  
**ICD (frozen by Integration):** `docs/icd/orbit_intercept_cue.md` · `interfaces/include/actprove/orbit_cue_msg.hpp`

---

## 1. OWNED (done)

| Item | Delivery |
|------|----------|
| Mission profile selector | GCS: `TEST_GEOMETRY` (0) · `AIRFRAME_JET` (1, gated) · `ORBIT_INTERCEPT` (2) |
| Manual orbit cue UI | Required `target_lat_deg` / `target_lon_deg` / `target_alt_m`; optional `R_orb_m`, `cue_unc_m`, `cue_unc_alt_m`, `target_speed_est_mps`, `clock_deg`, `alt_frame` |
| Align to Integration ICD | **No enum redefine** — consumes frozen `MissionProfile` / `OrbitCueMsg` |
| Option A note | Companion ingest of `OrbitCueMsg` + existing `USER_1` WORK — **no new MAVLink dialect** |
| Black-box kinds | `gcs_events`: `video_source_changed | profile_changed | cue_set` |
| TEST_GEOMETRY path | Preserved (commanded-miss demo); ABORT / FTS remain in UI |

## 2. Files touched

- `gcs-ui/src/types.ts` — `MissionProfile`, `OrbitCue`, helpers
- `gcs-ui/src/hooks/useMissionSim.ts` — profile + cue state; WORK requires cue for ORBIT_INTERCEPT
- `gcs-ui/src/components/ControlPanel.tsx` — profile row + orbit cue panel
- `gcs-ui/src/App.tsx` — wiring
- `gcs-ui/src/index.css` — cue panel styles
- `interfaces/schemas/orbit_cue_msg.json` — schema mirror of frozen header
- `onboard/logging/schema/blackbox_streams.json` + `src/blackbox.cpp` — event `detail` field
- `docs/SYSTEM_STACK.md` — pointer + Option A
- `docs/icd/README.md` — ICD index line
- `docs/SUPPLIER_ORBIT_INTERCEPT_SYS.md` — this ACK

## 3. BLOCKERS (open CONOPS — not System-owned)

| Open item | Owns |
|-----------|------|
| Default stand-off / orbit entry geometry (700–800 km/h vs TEST) | **Nav** (geometry already stamped — System does not invent) |
| Climb Δh rule `max(target_alt + Δh, min_safe_alt)` | **Nav** (ICD cites Δh=+200 m jet); **Safety / test** climb gate |
| Cue uncertainty → search box before Lock | **Nav** (defaults) · **Tracking** handoff |
| Click-to-lock vs AUTO detector (SEMI / MANUAL) | **Perception** · **Safety** policy |
| AIRFRAME_JET unlock | **Test** G3 green + mass / FTS |
| MAVLink Option B (`COMMAND_INT`) | **comms** (optional); Option A unblocks GCS now |
| PPS / HTE FAE buy-gate | **Integration** / hardware — unchanged |

## 4. Option A (System stack)

**Lab / GCS demo path (System OWN):**

1. Operator selects profile `ORBIT_INTERCEPT` (id=2) and enters cue fields.  
2. Companion (Orin) ingests `OrbitCueMsg` (shared memory / ROS / bridge) — **not** on video path.  
3. WORK remains existing `USER_1` param1=1.  
4. No new MAVLink dialect required for this path. TELEM1=RFD900 · TELEM2=Orin unchanged.

**Flight thin-C2:** ICD §4 Option B (`COMMAND_INT` + `USER_2` 31001) is **comms-owned** freeze. System GCS still authors the same `OrbitCueMsg` fields; bridge may encode Option A or B.

Nav climb-first / station / match / Lock⇒kill prosecute are **out of System OWN**.
