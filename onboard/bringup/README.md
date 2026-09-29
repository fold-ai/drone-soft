# onboard/bringup — Orin service start order

The checked-in services fail closed until the real PPS, MAVLink, and logging
daemons exist. For the host-only scaffold, run
`ACTPROVE_SIMULATE=1 ./onboard/bringup/scripts/start_order.sh`; never set that
flag on flight hardware.

**Owner:** System stack (+ hardware)  
**Docs:** `docs/bringup/orin_first_boot.md`, `mavlink_telem2_check.md`, `logger_smoke.md`

## Hard rule

**Do not start cameras / `detect_node` before PPS is ready** (`time_sync` disciplined).

## Order

1. `time_sync` — PPS ingest / `PpsClock`
2. `logging` — NVMe black-box under `/data/blackbox`
3. `mavlink_bridge` — UART MAVLink on FC **TELEM2** @ **57600** (TELEM1 = RFD900x)
4. `safety_gates` — mission SM stubs (BOOT…FTS); policy owned by #safety
5. Perception hooks — `vision/apps/detect_node` (owned by perception)

## Scripts

```bash
# Explicit host scaffold only
ACTPROVE_SIMULATE=1 ./onboard/bringup/scripts/start_order.sh
./onboard/bringup/scripts/stop_all.sh
```

## systemd

Units under `systemd/` (install to `/etc/systemd/system/` on Orin):

- `actprove-time-sync.service`
- `actprove-logging.service`
- `actprove-mavlink.service`

Enable after Integration PASS on PPS/FSYNC path:

```bash
sudo systemctl enable --now actprove-time-sync actprove-logging actprove-mavlink
```

## Launch XML

`launch/onboard_stack.launch.xml` — optional ROS2-style sketch; systemd preferred on vehicle.
