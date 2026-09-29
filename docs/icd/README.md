# ICD — Interface Control Documents (V1)

**Owner:** System stack  
**Freeze:** align with V1_PLAN v0.1 (Fri 2026-09-18)

| Doc | Contract |
|-----|----------|
| [`detection_msg.md`](detection_msg.md) | Perception → Tracking boxes |
| [`track_msg.md`](track_msg.md) | Tracking → Navigation track (`measurement_epoch` required) |
| [`time_sync.md`](time_sync.md) | PPS domain, mid-exposure, HW-ts |
| [`latency_budget.md`](latency_budget.md) | camera→Orin ≤30–40 ms; E2E ≤50–80 ms; frame→box <40 ms p95 |
| [`mavlink_v1.md`](mavlink_v1.md) | Orin↔FC common.xml v1 cmds + setpoints (TELEM2) |
| [`ground_eo_handoff.md`](ground_eo_handoff.md) | Ground EO cue → onboard track handoff |
| [`gcs_video_bda.md`](gcs_video_bda.md) | GCS Nose\|Ground video + operator BDA contract |
| [`guidance_msg.md`](guidance_msg.md) | Commanded-miss / heading (Navigation ICD pointer) |

**Rules**

- `measurement_epoch` = PPS-domain time of frames that formed the update (`t_pps` alias OK if equal).
- Soft host `now()` forbidden for sensor / track epochs.
- No custom MAVLink dialect. No video on C2.
- V1 intent: see → lock → heading → commanded miss @ ~100 km/h — **no combat intercept / kill / stick**.
