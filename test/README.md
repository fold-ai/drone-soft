# Test

`integration/host_chain_smoke.cpp` verifies the host-side Detection → Tracking
→ Safety → Navigation → MAVLink-struct contract. It is explicitly not a
substitute for Argus/TensorRT/PX4 hardware evidence or an FC log.

| Path | What |
|------|------|
| [`g1_g3/`](g1_g3/) | Gates G1–G3 criteria, log layout, scorers |
| [`g_range_no_radar/`](g_range_no_radar/) | **G-RANGE-NR-V1** — no-radar ground EO cue → Lock → CLOSE → BDA (`TARGET_DESTROYED`→RTB or `MISS`/`REATTACK`→re-WORK). Spec: `docs/notes/GROUND_EO_CUE_AND_BDA.md` |
| [`recognition_gate/`](recognition_gate/) | **REC-TRAIN-GATE-V1** — mAP / class confusion / FP-on-negatives (weights gate for Lock) |
