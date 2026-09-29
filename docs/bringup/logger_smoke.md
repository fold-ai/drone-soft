# Logger Smoke (NVMe black-box)

**Path:** `onboard/logging` → `/data/blackbox/YYYYMMDD_HHMMSS_<id>/`  
**Capacity note:** size NVMe for **≥1–2 h** continuous V1 rates.

## Smoke

```bash
cmake -S onboard -B onboard/build && cmake --build onboard/build -j --target logger_smoke

# Host
./onboard/build/logger_smoke --out /tmp/actprove_blackbox --seconds 10

# Orin
./onboard/build/logger_smoke --out /data/blackbox --seconds 10
```

Validate ICD fields:

```bash
python3 tools/log_replay/log_replay.py /tmp/actprove_blackbox/<run_id>
```

## Pass

1. Folder created with `meta.json` + JSONL streams (`camera_ts`, `tracks`, `mission_state`).
2. Sustained write for soak without blocking capture thread (async queue — TODO ring).
3. Schema fields present: `measurement_epoch_ns` on camera/tracks; see `onboard/logging/schema/blackbox_streams.json`.
4. Optional soak: ≥10 min continuous write then `log_replay --strict`.

See `test/g1_g3/README.md` for G1 folder layout.
