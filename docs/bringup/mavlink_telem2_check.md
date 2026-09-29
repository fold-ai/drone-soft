# MAVLink TELEM2 Check

**Path:** Orin UART ↔ FC **TELEM2** · **Baud:** **57600** (match FC) · **Dialect:** common.xml v1  
**TELEM1** = RFD900x GCS C2 — do not use for this test.  
**No video** on this link.

## Smoke

```bash
cd /opt/drone-soft   # or workspace clone
cmake -S onboard -B onboard/build && cmake --build onboard/build -j --target mavlink_bridge_smoke

# Default baud 57600 per ICD / interfaces/mavlink/telem_map.h
./onboard/build/mavlink_bridge_smoke --port /dev/ttyTHS1 --baud 57600 --smoke
```

With FC powered and TELEM2 harness connected, also watch QGC / `mavlink-routerd`.

## Pass criteria

1. HEARTBEAT Orin→FC at 1 Hz visible in QGC / FC log.
2. HEARTBEAT FC→Orin received; link age <1 s.
3. NAMED_VALUE_INT `MISSION_STATE=0` (BOOT) published.
4. COMMAND_LONG WORK (USER_1 param1=1) ACKed; state → SEARCH when BIT OK.
5. Lost HEARTBEAT inject **3 s** → FC/safety path selects **RTB** (not FTS).

## Fail / debug

| Symptom | Check |
|---------|-------|
| open() fails | Port name, udev, wiring to TELEM2 not TELEM1 |
| No HB on FC | Baud mismatch (57600), TX/RX swap, common ground |
| Video/images on link | **Wrong** — video is separate RF |

See `docs/icd/mavlink_v1.md`, `onboard/mavlink_bridge/README.md`.
