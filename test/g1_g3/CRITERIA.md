# Gate criteria (acceptance)

Aligned with V1_PLAN §7 + SAFETY V1. States: `BOOT` `SEARCH` `LOCK` `CLOSE` `ABORT` `RTB` `FTS`.  
Ground cmds (exact): `WORK` `ABORT` `RTB` `FTS`.

---

## G1 — Bench (perception + timebase only)

**Phase:** see (detect)  
**Not in G1:** WORK/ABORT cmd decode (starts at G2).

### Pass (all required)

| ID | Criterion | Metric |
|----|-----------|--------|
| G1-Hz | Detector sustained rate | ≥ **60 Hz** (not burst) |
| G1-Lat | Hot path latency | **frame → box** p95 **&lt; 40 ms** (same clock as `t_pps`) |
| G1-PPS-Cam | Camera timebase | PPS mid-exposure on every scored frame (`DetectionMsg.t_pps`) |
| G1-PPS-IMU | IMU timebase | PPS / DRDY HW-ts present (soft `now()` = FAIL) |

### Evidence

- Run folder: `G1_YYYYMMDD_HHMMSS_<id>/` (see `LOG_LAYOUT.md`)
- Streams: `camera_ts`, `boxes` (`DetectionMsg`), `frame_to_box_ms`, detector Hz, `imu_ts` + sync status
- Summary: `summary.json` matching `schemas/g1_summary.schema.json`

### Abort / FAIL

- PPS unlock &gt; 1 s continuous
- Hz or latency fail for **2 consecutive 30 s** windows
- Thermal/throttle that breaks 60 Hz
- GUI-only claim without archive → **FAIL**

---

## G2 — Iron bird (ground; engine may be off)

**Phase:** see → lock → heading  
**Requires:** named FTS holder if RF/arming paths can be live.

### Pass (all required)

| ID | Criterion |
|----|-----------|
| G2-FC | Box path → MAVLink heading (agreed setpoint, e.g. `SET_POSITION_TARGET_LOCAL_NED`) **visible in FC log** |
| G2-WORK | `WORK` is the only entry to `SEARCH` after `BOOT` or ABORT-clear; SEARCH without WORK = **FAIL** |
| G2-SM | States logged with SAFETY enum only |
| G2-FTS* | If FTS scripted: throttle idle + surfaces fixed + fuel-cut relay asserted; **no explosive path** |

### Evidence

- All G1 streams (if cams/IMU in loop) + `lock_state` + MAVLink setpoints (tx time) + **FC log excerpt**
- `summary.json` → `schemas/g2_summary.schema.json`

### Abort / FAIL

- Unexpected arm / motor spin
- Heading only on GCS, not in FC log
- Lost-link handling that fires FTS without FTS cmd / hard fail (must be **RTB**)
- Unnamed FTS holder when RF/arming live → **no run**

---

## G3 — Flight (cooperative ~100 km/h)

**Phase:** see → lock → heading → commanded miss  
**Requires:** G2 green path + named FTS holder.

### Pass (per sortie; need **10** valid)

| ID | Criterion |
|----|-----------|
| G3-Tgt | Cooperative target ~**100 km/h** |
| G3-Own | Own-ship **150–200 km/h max** (not 800) |
| G3-Lock | Lock **8–15 s** counted in **`LOCK` only** (SEARCH does not pad) |
| G3-Miss | Commanded miss **15–30 m** (miss doctrine — hit/kill profile = **FAIL**) |
| G3-N | **10** archived sorties meeting all of the above |

### Evidence

- Full log list + FC log + sortie sheet + miss geometry source labeled
- `summary.json` → `schemas/g3_summary.schema.json` + `sorties.jsonl`

### Abort / day-kill

- Own-ship &gt; 200 km/h requested or observed
- Push toward Shahed-class speed
- FTS holder absent / unbriefed
- Corridor-less new-target hunt or terminal stick/kill guidance

---

## Cross-cutting SAFETY (G2+)

| Event | Required behavior |
|-------|-------------------|
| Lost-link (e.g. 3 s no HEARTBEAT) | **RTB** (not FTS unless FTS cmd or hard fail) |
| Lost-track (no box) | → **SEARCH**; SEARCH timeout → **ABORT** |
| FTS cmd | Idle + surfaces fixed + fuel-cut relay; non-explosive |
| After BOOT or ABORT clear | Only **`WORK`** enters **SEARCH** |

---

## Envelope rule

**G3 red ⇒ no own-ship speed increase.**  
Only G3 green (10 valid sorties) unlocks any higher cooperative-speed work.

---

## Related: G-RANGE-NR-V1 (no radar)

Range procedure card (English): [`../g_range_no_radar/TEST_CARD.md`](../g_range_no_radar/TEST_CARD.md).  
Flow: ground EO → Lock → CLOSE → `TARGET_DESTROYED`→RTB **or** `MISS`/`REATTACK`→re-WORK.  
Does not replace G1–G3; does not raise speed while G3 is red. Requires named FTS holder.
