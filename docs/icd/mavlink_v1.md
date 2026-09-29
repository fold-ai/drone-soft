# MAVLink V1 ICD (Orin ↔ FC ↔ GCS)

**Owner:** #comms  
**Authority:** `V1_PLAN.md` §5 (freeze-aligned with #safety)  
**Dialect:** MAVLink **common.xml v1 only** — no custom dialect, no custom binary framing.  
**Intent:** see / hold / commanded miss distance — **not kill**.

---

## Physical / TELEM map

| Port | Role | Baud / air | Notes |
|------|------|------------|-------|
| **TELEM1** | RFD900x-class C2 (900 MHz) | **57600** UART; **~64 kbps** air | Wingtip-only antenna per airframe ICD. Primary GCS↔FC C2. |
| **TELEM2** | Orin UART MAVLink | **57600** (match FC SERIALn) | Onboard peer; Orin `compid=191` (onboard computer). |
| Video RF | Near-field digital 1.2 or 2.4 GHz | separate | Rail/tower only. **Not** FC UART. **Not** MAVLink. |

**IDs:** vehicle `sysid=1`; Orin companion `compid=191`; GCS `sysid=255`.  
**Flow control:** none on TELEM1/TELEM2. Prefer separate BEC for RFD900x (do not starve FC 5 V).

```
[GCS] --RF 900 MHz-- [RFD900x air] --UART-- FC TELEM1
                                            FC TELEM2 --UART-- [Orin]
[rail/tower Rx] --RF 1.2/2.4-- [air Tx] <-- cam/Orin (not MAVLink)
```

---

## Message table

| Message | ID | Dialect | Direction | Rate | Purpose | V1 |
|---------|----|---------|-----------|------|---------|----|
| HEARTBEAT | 0 | common | Orin↔FC↔GCS | 1 Hz | Liveness, mode/armed; lost Orin HB **3 s → RTB** | Y |
| SET_POSITION_TARGET_LOCAL_NED | 84 | common | Orin→FC | 10–20 Hz | Heading + climb / commanded-miss (vel+yaw type_mask; **not kill**) | Y |
| COMMAND_LONG (WORK) | 76 | common | Orin\|GCS→FC | event | `MAV_CMD_USER_1` (31000) `param1=1` | Y |
| COMMAND_LONG (ABORT) | 76 | common | Orin\|GCS→FC | event | `MAV_CMD_USER_1` (31000) `param1=2` — **≠ FTS** | Y |
| COMMAND_LONG (RTB) | 76 | common | Orin\|GCS→FC | event | `MAV_CMD_NAV_RETURN_TO_LAUNCH` (20) | Y |
| COMMAND_LONG (FTS) | 76 | common | Orin\|GCS→FC | event | `MAV_CMD_DO_FLIGHTTERMINATION` (185); **never auto on lost-link** | Y |
| COMMAND_ACK | 77 | common | FC→Orin,GCS | event | Ack WORK/ABORT/RTB/FTS | Y |
| NAMED_VALUE_INT (`MISSION_STATE`) | 252 | common | Orin→FC,GCS | on change / 2 Hz | name=`MISSION_STATE`; enum 0–6 | Y |
| VFR_HUD | 74 | common | FC→Orin,GCS | 5 Hz | Airspeed, heading, alt, climb | Y |
| BATTERY_STATUS | 147 | common | FC→Orin,GCS | 1 Hz | Energy | Y |
| SYS_STATUS | 1 | common | FC→Orin,GCS | 1 Hz | Health / drop rate | Y |
| GPS_RAW_INT | 24 | common | FC→**GCS only** | 1 Hz | Ground awareness; **Orin must not use for guidance** | Y |
| STATUSTEXT | 253 | common | any→GCS | event | Human faults; not control | N |

---

## Uplink command names (frozen — no aliases)

| Name | MAVLink | Meaning |
|------|---------|---------|
| **WORK** | USER_1 / param1=1 | Enter/continue mission (BOOT→SEARCH or resume after ABORT-clear) |
| **ABORT** | USER_1 / param1=2 | End engagement; stop CLOSE/LOCK; do not hunt. **Not** FTS. |
| **RTB** | NAV_RETURN_TO_LAUNCH (20) | Return / recovery route |
| **FTS** | DO_FLIGHTTERMINATION (185) | Non-explosive: throttle idle + fixed surfaces + fuel-cut relay. Commanded-only. |

### BDA uplink names (FROZEN with #safety / #pm — no aliases)

Operator BDA / ground cmds only — **not** weapon release / not terminal stick. Spec: `docs/notes/GROUND_EO_CUE_AND_BDA.md`.

| Name | USER_1 param1 | SM effect (V1) |
|------|--------------:|----------------|
| **TARGET_DESTROYED** | **3** | Clear engagement → **RTB always** (not LAND_SOFT even if `TEST_RECOVER`). Not auto-kill. |
| **MISS/REATTACK** | **4** | Clear CLOSE/engagement; ABORT cleared; allow new WORK / re-cue. Not kill. |

Wire/UI string exactly `MISS/REATTACK`. C macro: `AP_UPLINK_MISS_REATTACK` (=4).  
**Do not use param1=5** — not on the wire.

**TARGET_DESTROYED ≠ FTS ≠ ABORT ≠ MISS/REATTACK.** No aliases.

Do **not** alias (no “RTL”, “kill”, “terminate” as ABORT, etc.) in docs, UI, or bridge code.

---

## SET_POSITION_TARGET_LOCAL_NED (heading + commanded miss)

- **type_mask:** velocity + yaw only (ignore position / accel / force / yaw_rate as appropriate).
- `vx`/`vy` from desired ground-track heading; `vz` = climb rate; `yaw` = desired heading.
- Express closing geometry to **miss distance** (G3: 15–30 m), never impact.
- Rate **10–20 Hz** while in CLOSE; stop publishing intercept setpoints on ABORT/RTB/FTS or lost-link RTB.

---

## Mission state enum (`NAMED_VALUE_INT`)

`name` field exactly: `MISSION_STATE`

| Value | State |
|------:|-------|
| 0 | BOOT |
| 1 | SEARCH |
| 2 | LOCK |
| 3 | CLOSE |
| 4 | ABORT |
| 5 | RTB |
| 6 | FTS |

Source of truth for state machine: #safety §6.

---

## Pub / sub

| Node | Publishes | Subscribes |
|------|-----------|------------|
| **FC** | HEARTBEAT, VFR_HUD, BATTERY_STATUS, GPS_RAW_INT, SYS_STATUS, COMMAND_ACK | HEARTBEAT (Orin), SET_POSITION_TARGET_LOCAL_NED, COMMAND_LONG |
| **Orin** | HEARTBEAT, SET_POSITION_TARGET_LOCAL_NED, MISSION_STATE, WORK/ABORT/RTB/FTS, TARGET_DESTROYED, MISS/REATTACK | HEARTBEAT (FC), VFR_HUD, BATTERY_STATUS, SYS_STATUS, COMMAND_ACK |
| **GCS** | HEARTBEAT, WORK/ABORT/RTB/FTS + BDA (TARGET_DESTROYED, MISS/REATTACK) | Telemetry + MISSION_STATE (optional setpoint mirror) |

---

## Lost-link / fail-safe

**N = 3 s** without Orin `HEARTBEAT` on the vehicle MAVLink bus → FC **RTB**.  
FTS is **commanded-only** (or hard irrecoverable per #safety) — **never** automatic on lost-link.

---

## Explicitly NOT on C2 / MAVLink

- HD / any video frames or codec bitstream  
- Raw images, detection ROIs as blobs  
- Map tiles, logs, core dumps  
- Starlink / phone LTE / raw 5.8 analog as the control path  
- High-rate IMU or camera metadata (stays on Orin↔sensor bus)

C2 carries setpoints, mission state, commands, and thin telemetry only.

---

## Code constants

See [`interfaces/mavlink/`](../../interfaces/mavlink/) — `mavlink_v1_cmds.h`, `mission_state.h`, `telem_map.h`.
