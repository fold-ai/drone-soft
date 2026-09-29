# Latency Budget ICD

**Owner:** System stack (+ perception + #test)  
**Throughput:** ≥60 Hz sustained on detect path.

## Gates (V1 hard)

| Gate | Budget | Probe |
|------|--------|-------|
| **camera → Orin** (mid-exposure / SOF → buffer ready on Orin) | **≤ 30–40 ms** | `tools/latency_probe` `--camera-jsonl` |
| **frame → box** (mid-exposure `t_pps` → DetectionMsg publish) | **p95 < 40 ms** | perception `latency_bench` + G1 |
| **E2E capture → track publish** (`measurement_epoch` → TrackMsg publish) | **≤ 50–80 ms** | `tools/latency_probe` `--track-jsonl` |

## Perception pipeline (frame→box breakdown)

| Stage | Budget |
|-------|--------|
| Capture dequeue + PPS stamp | ~1–2 ms |
| CUDA preprocess (letterbox + normalize) | ~1–2 ms |
| TensorRT infer (YOLOv8n @ 640 FP16) | ≤20–25 ms |
| Postprocess + publish | ~1–2 ms |

## Notes

- 60 Hz period = 16.7 ms; pipeline latency may span multiple frames if throughput ≥60 Hz and age within budget.
- Drop policy: detect backlog >1 → drop oldest capture (never stall camera thread).
- If infer alone >30 ms → try 512 input or INT8 before changing detector family.
- G1 gate: detector ≥60 Hz sustained; frame→box <40 ms p95; PPS on frames **and** IMU.
- Timestamps must be PPS-domain `measurement_epoch` — soft `now()` fails the gate.
