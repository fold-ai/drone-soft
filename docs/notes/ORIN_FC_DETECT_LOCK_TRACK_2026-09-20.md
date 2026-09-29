# Orin + FC — detect / lock / track improve (PM 2026-09-20)

User whiteboard (MacBook Air): **Orin** = companion (2 cams → detect / capture / track); **FC** = GPS + telem + 12ch Rx + motors.
Links Orin↔FC: **SBUS** (Orin as synthetic RX into FC) + **MAVLink** (bidirectional).

Repo on Mac: `~/Downloads/actprove-drone` (no git remote). Existing: `vision/apps/dual_detect_node`, `tools/nose_eo_*`, tracking ICDs, Engage/Kill HITL freezes.

## Product freezes still in force
- MANUAL → TEST → SEARCH → LOCK_HOLD (red box) → WAIT_AUTHORIZE → Kill → CLOSE → LAND
- ELRS 12ch Rx = pilot + abort (always wins)
- Kill authorize before CLOSE
- Cloud out of C2

## Channel OWN tasks
See fan-out messages.
