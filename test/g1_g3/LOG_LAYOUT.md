# Log / evidence layout

## Run folder naming

```
GATE_YYYYMMDD_HHMMSS_<runid>/
```

Examples: `G1_20260918_143000_bench01/`, `G2_20260920_090015_iron01/`, `G3_20260925_111200_s03/`

Root for archives (on aircraft NVMe or bench host): `logs/gates/` (symlink or mount OK).

## Required files (every gate)

| File | Required | Notes |
|------|----------|-------|
| `manifest.json` | yes | schema: `schemas/run_manifest.schema.json` |
| `summary.json` | yes | gate-specific summary schema |
| `PASS` or `FAIL` | yes | empty marker file written **only by TEST** after score |
| `notes.md` | optional | human context; never substitutes for streams |

## Stream files

| Stream | Path | Gates | Schema / ICD |
|--------|------|-------|--------------|
| Camera / detect | `streams/detections.jsonl` | G1+ | `DetectionMsg` (`t_pps`, `cam_id`, `src_w/h`, `boxes[32]`) + `frame_to_box_ms` |
| Detector stats | `streams/detector_hz.jsonl` | G1+ | windowed Hz samples |
| IMU | `streams/imu.jsonl` | G1+ | `imu_ts` (PPS), ω, a; sync_ok flag |
| Sync health | `streams/sync_status.jsonl` | G1+ | PPS lock / unlock events |
| Air data | `streams/airspeed.jsonl` | G2* / G3 | airspeed or labeled GPS groundspeed |
| MAVLink setpoints | `streams/mavlink_setpoints.jsonl` | G2+ | heading/climb cmds + tx time |
| Lock / SM | `streams/lock_state.jsonl` | G2+ | SAFETY enum + cmd that caused transition |
| FC excerpt | `fc/fc_excerpt.ulg` or `.log` + `fc/heading_proof.txt` | G2+ | must show heading in **FC** log |
| FTS path | `fc/fts_assert.json` | G2 if scripted / G3 | idle, surfaces, fuel-cut |
| Sortie sheet | `sorties.jsonl` | G3 | one line per sortie attempt |
| Miss geometry | `streams/miss_geometry.jsonl` | G3 | commanded miss m + source label |

\* G2: airspeed may be N/A on ground — set `"airspeed_source": "na_ground"` in manifest; do not invent values.

## Detection line (jsonl)

One object per processed frame (empty `n=0` still logged):

```json
{
  "t_pps": 1710000000.012,
  "seq": 12345,
  "cam_id": 0,
  "src_w": 1920,
  "src_h": 1200,
  "n": 1,
  "boxes": [{"x": 10, "y": 20, "w": 30, "h": 40, "conf": 0.9, "class_id": 0}],
  "frame_to_box_ms": 22.4
}
```

## lock_state line (jsonl)

```json
{
  "t_pps": 1710000000.100,
  "state": "LOCK",
  "prev": "SEARCH",
  "cmd": "WORK",
  "reason": "track_confirm"
}
```

`state` ∈ `BOOT|SEARCH|LOCK|CLOSE|ABORT|RTB|FTS`.  
`cmd` ∈ `WORK|ABORT|RTB|FTS|null` (null = autonomous transition).
