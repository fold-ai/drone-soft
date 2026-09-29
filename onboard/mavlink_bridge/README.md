# mavlink_bridge — Orin ↔ FC TELEM2

**Owner:** System stack (+ #comms)  
**ICD:** `docs/icd/mavlink_v1.md` · constants: `interfaces/mavlink/`

| Setting | Value |
|---------|-------|
| Port | FC **TELEM2** UART (Orin `/dev/ttyTHS*` typical) |
| Baud | **57600** (`AP_TELEM_BAUD`) — match FC SERIALn |
| TELEM1 | RFD900x GCS C2 — **not** this bridge’s UART |
| Dialect | common.xml MAVLink v1 only |
| Video | **NOT** on MAVLink |

Implements MAVLink v1 framing and CRC for HEARTBEAT, SET_POSITION_TARGET_LOCAL_NED,
COMMAND_LONG WORK/ABORT/RTB/FTS + operator BDA, COMMAND_ACK/COMMAND_LONG receive
parsing, and NAMED_VALUE_INT mission state telemetry. The wire field in
NAMED_VALUE_INT is limited by MAVLink to 10 bytes and is therefore `MISSION_ST`;
the application/logging name remains `MISSION_STATE`.

`DO_FLIGHTTERMINATION` transmission is disabled by default and requires the
explicit `BridgeConfig::allow_flight_termination` hardware-test gate.

## Operator BDA uplink (GCS → Orin → safety_gates)

GCS C2 may send these on **TELEM1** (RFD900x). The Orin bridge must **accept and forward**
into `safety_gates` as the **exact frozen strings** below (no aliases).

| Bridge API | USER_1 param1 | safety_gates string | SM effect (V1) |
|------------|--------------:|---------------------|----------------|
| `send_target_destroyed()` | **3** | `TARGET_DESTROYED` | Clear engagement → **RTB always** (never LAND_SOFT) |
| `send_miss()` | **4** | `MISS/REATTACK` | Clear CLOSE → **ABORT**; allow new WORK |
| `send_reattack()` | **4** | `MISS/REATTACK` | V1 same semantics as miss (frozen ICD: **no param1=5**) |

Constants: `AP_UPLINK_TARGET_DESTROYED`, `AP_UPLINK_MISS_REATTACK` in `interfaces/mavlink/mavlink_v1_cmds.h`.  
Spec: `docs/notes/GROUND_EO_CUE_AND_BDA.md`, `docs/icd/gcs_video_bda.md`.

```bash
cmake -S onboard -B onboard/build && cmake --build onboard/build -j --target mavlink_bridge_smoke
./onboard/build/mavlink_bridge_smoke --port /dev/ttyTHS1 --baud 57600 --smoke
```

The bridge is self-contained for this frozen MAVLink v1 subset; `c_library_v2`
may still be used later if the message set expands.
