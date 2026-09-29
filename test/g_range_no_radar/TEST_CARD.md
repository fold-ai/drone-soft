# Test card — G-range no-radar

**ID:** G-RANGE-NR-V1  
**Owner:** test (pass/fail)  
**Language:** English (GCS labels + this card)  
**Spec parents:** [`docs/notes/GROUND_EO_CUE_AND_BDA.md`](../../docs/notes/GROUND_EO_CUE_AND_BDA.md), [`docs/notes/MANUAL_LOCK_AND_TEST_LAND.md`](../../docs/notes/MANUAL_LOCK_AND_TEST_LAND.md), SAFETY V1, V1_PLAN §7  
**Radar:** **none** — fail the run if any radar cue is used.

## Purpose

Range test when interceptor and cooperative/surrogate target share geography and **no radar** is available: ground EO cues the stack, operator Manual Lock, CLOSE attempt, then **operator BDA** decides RTB vs reattack.

## Preconditions (go)

| # | Check | Owner |
|---|--------|-------|
| 1 | Named **FTS holder** present and briefed | test + safety |
| 2 | Corridor loaded; C2 (RFD900) healthy | comms + safety |
| 3 | Ground EO → GCS ingest live (USB/V4L2 or approved capture); source toggle shows **Ground EO** | perception + GCS |
| 4 | Nose EO available for handoff when target enters FOV | hardware + perception |
| 5 | `TEST_RECOVER` set per range SOP (1 = soft-land after engagement; 0 = RTB/ditch) **before WORK** | safety + nav |
| 6 | No radar in the loop (config + verbal confirm) | test |

**No-go:** FTS holder unnamed; corridor missing; Ground EO dark; WORK before BOOT clear; radar enabled.

## Scripted sequence

```
Ground EO aim → Manual Lock (image box) → launch / WORK → SEARCH
  → onboard acquire / LOCK (handoff) → CLOSE (commanded miss)
  → operator BDA:
       Mark destroyed  → TARGET_DESTROYED → RTB (or LAND_SOFT if TEST_RECOVER=1)
       Mark miss       → MISS / REATTACK → clear CLOSE → re-WORK (second pass) or RTB per SOP
```

### Step detail

| Step | Action | Expected SM / behavior |
|------|--------|-------------------------|
| 1 | Operator selects **Ground EO**, aims at target | Video live; no Lock yet |
| 2 | **Manual Lock** (tap/box on ground EO) | `operator_lock_request` with valid image box — **not** map pin |
| 3 | GCS sends cue + issues **WORK** (launch/turn as briefed) | BOOT/ABORT-clear → **SEARCH** only via WORK |
| 4 | Target enters nose FOV; onboard track takes over | LOCK (or SEARCH→LOCK per SM rules); ground seed → onboard track |
| 5 | **CLOSE** | Commanded miss profile — **no** terminal stick / kill guidance |
| 6a | **Mark destroyed** | `TARGET_DESTROYED` → clear engagement → **RTB** (or ABORT+LAND_SOFT if `TEST_RECOVER=1`) |
| 6b | **Mark miss / reattack** | `MISS` / `REATTACK` → clear CLOSE → **re-WORK** (or RTB) — **do not** auto-declare kill |
| 7 | Recovery | Per `TEST_RECOVER`; FTS only on FTS cmd / hard fail |

## Pass criteria (all required)

| ID | Criterion |
|----|-----------|
| P1 | Full sequence completed with **no radar** |
| P2 | Manual Lock used **image box** on Ground EO (map-pin-only Lock = FAIL) |
| P3 | `WORK` gated SEARCH; SEARCH without WORK = FAIL |
| P4 | CLOSE executed as **commanded miss** (hit/kill profile = FAIL) |
| P5 | Operator BDA recorded: either `TARGET_DESTROYED`→RTB/LAND_SOFT **or** `MISS`/`REATTACK` with no auto-kill |
| P6 | Lost-link (if injected) → **RTB**, not FTS, unless FTS cmd / hard fail |
| P7 | Archive complete per logging fields below; TEST writes `PASS` or `FAIL` marker |

**Campaign note:** This card is a **range procedure** gate, not a substitute for G3’s 10-sortie lock/miss envelope. G3 remains red until its own criteria pass; this card does not raise own-ship speed.

## Fail / abort

- Radar or map-pin Lock used  
- Auto kill declaration without operator BDA  
- Terminal stick / kill guidance  
- Unexpected arm, corridor violation, or FTS without cmd/hard fail  
- Missing FTS holder  
- Incomplete logs  

## Logging fields (required archive)

Run folder: `GRANGE_YYYYMMDD_HHMMSS_<id>/` under `logs/gates/` (same discipline as `test/g1_g3/LOG_LAYOUT.md`).

### `manifest.json`

| Field | Type | Notes |
|-------|------|-------|
| `card_id` | string | `G-RANGE-NR-V1` |
| `run_id` | string | |
| `started_at` | string | ISO-8601 |
| `operator` | string | GCS operator |
| `fts_holder` | string | **required** (named) |
| `radar_used` | boolean | must be `false` |
| `eo_source` | string | `ground` at Lock time |
| `test_recover` | integer | `0` or `1` |
| `git_sha` | string | |

### Stream / event fields

| Field | Where | Notes |
|-------|-------|-------|
| `t_pps` / `camera_ts` | detections / video meta | Ground EO + nose when active |
| `cam_id` / `eo_source` | detections | Distinguish ground vs nose |
| `manual_lock_box` | events | xywh + `t_pps` + source=`ground` |
| `operator_lock_request` | events | edge time + accepted/rejected |
| `cmd` | lock_state / cmds | `WORK` `ABORT` `RTB` `FTS` + BDA cmds |
| `lock_state` | lock_state.jsonl | SAFETY enum only |
| `bda_mark` | events | `TARGET_DESTROYED` \| `MISS` \| `REATTACK` + operator id + time |
| `close_miss_m` | miss_geometry | commanded miss if measured; else `na` + reason |
| `mavlink_setpoints` | streams | heading/climb during CLOSE |
| `airspeed` / IAS | streams | labeled source |
| `rtb_or_land_soft` | summary | which recovery path ran |
| `land_soft_active` | summary | if `TEST_RECOVER=1` |

Schema twin: `schemas/grange_manifest.schema.json`, `schemas/grange_summary.schema.json`.

## People

| Role | Duty |
|------|------|
| TEST | Pass/fail, archive, veto |
| GCS operator | Ground EO aim, Manual Lock, BDA marks |
| Pilot | Launch / airframe |
| FTS holder | Kill authority |
| Safety / range | Abort words, corridor |

## Abort words

**Abort** / **FTS** / **Knock it off** — immediate. No debate on the net.

## Related G-gates

| Gate | Relationship |
|------|----------------|
| G1–G3 | Envelope / evidence gates in [`../g1_g3/`](../g1_g3/). This card does **not** replace them. |
| G3 red | Own-ship speed stays frozen; this range card does not unlock higher speed. |
| Design | [`docs/notes/GROUND_EO_CUE_AND_BDA.md`](../../docs/notes/GROUND_EO_CUE_AND_BDA.md) |

