# latency_probe — camera→Orin & E2E latency gates

**Owner:** System stack (+ #test)  
**ICD:** `docs/icd/latency_budget.md`

| Gate | Budget |
|------|--------|
| camera → Orin | ≤ **30–40 ms** |
| E2E capture → track publish | ≤ **50–80 ms** |
| frame → box (perception) | p95 **< 40 ms** |

## Run

```bash
# Demo (no log files)
python3 tools/latency_probe/latency_probe.py --demo

# Against black-box JSONL
python3 tools/latency_probe/latency_probe.py \
  --camera-jsonl /data/blackbox/RUN/camera_ts.jsonl \
  --track-jsonl  /data/blackbox/RUN/tracks.jsonl
```

Stdlib only; optional `numpy` improves percentile math.
