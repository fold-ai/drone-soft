# onboard/ — System stack hot-path packages

**Owner:** System stack  
**V1 intent:** see → lock → heading → commanded miss @ ~100 km/h (no combat intercept / kill).

| Package | Role |
|---------|------|
| [`bringup/`](bringup/) | Start order, systemd, launch — **PPS before cameras** |
| [`time_sync/`](time_sync/) | `PpsClock` / PPS ingest / mid-exposure stamps |
| [`logging/`](logging/) | NVMe black-box (≥1–2 h) |
| [`mavlink_bridge/`](mavlink_bridge/) | Orin↔FC UART MAVLink on **TELEM2 @ 57600** |
| [`companion/`](companion/) | Lock/Takeover loop: IPC detections → GUIDED body velocity |
| [`safety_gates/`](safety_gates/) | Mission SM stubs BOOT…FTS (#safety owns policy) |

## Build

```bash
cmake -S onboard -B onboard/build
cmake --build onboard/build -j
./onboard/build/logger_smoke --out /tmp/actprove_blackbox
./onboard/build/mavlink_bridge_smoke --port /dev/ttyTHS1 --baud 57600 --smoke
./onboard/build/actprove_companion --smoke
ctest --test-dir onboard/build --output-on-failure
```

ICD: `docs/icd/`. Bring-up: `docs/bringup/` + `bringup/scripts/start_order.sh`.
