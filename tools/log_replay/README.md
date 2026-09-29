# log_replay — offline ICD validation

**Owner:** System stack + #test  
**ICD:** `docs/icd/`, `onboard/logging/schema/blackbox_streams.json`

Loads a black-box run folder and checks required streams / fields
(`measurement_epoch_ns` on camera_ts + tracks, etc.). Does **not** re-simulate flight.

## Run

```bash
python3 tools/log_replay/log_replay.py /data/blackbox/YYYYMMDD_HHMMSS_run
python3 tools/log_replay/log_replay.py /path/to/run --strict
```
