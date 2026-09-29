# COMMS Audit — MAVLink ICD, TELEM (N250 install), RFD900 + near-field video

**Owner:** #comms  
**Date:** 2026-09-16  
**Scope:** Verify frozen MAVLink V1 (WORK/ABORT/RTB/FTS + BDA 3/4), TELEM map for FC install, RFD900 C2 + near-field video. English.  
**Sources:** `docs/icd/mavlink_v1.md`, `interfaces/mavlink/*`, `V1_PLAN.md` §5, `docs/notes/GROUND_EO_CUE_AND_BDA.md`, `docs/icd/gcs_video_bda.md`, `BOM-v0.1.md`  
**Method:** Doc/header cross-check + virtual consistency tests (no RF range run).

---

## Verdict

| Area | Result | Notes |
|------|--------|-------|
| MAVLink dialect | **PASS** | common.xml v1 only; no custom dialect |
| Uplink WORK/ABORT/RTB/FTS | **PASS** | USER_1 1/2; RTB=20; FTS=185; names frozen |
| BDA TARGET_DESTROYED / MISS/REATTACK | **PASS** | param1 **3** / **4** only; **no param1=5** |
| TELEM1 / TELEM2 map | **PASS** | TELEM1=RFD900 C2; TELEM2=Orin @ 57600 |
| RFD900 C2 | **PASS** | 900 MHz; ~64 kbps; wingtip ant; BUY |
| Near-field video | **PASS (SKU open)** | Separate 1.2/2.4 GHz RF; not on MAVLink/C2 |
| N250-specific pinout | **OPEN** | String `N250` not in tree; TELEM map stamped for Pixhawk 6X / Cube Orange+ class — confirm if N250 ≠ that class |

**Overall:** **PASS with OPEN** (video SKU + N250 identity). Safe to sync ICD to Downloads; do not allocate param1=5.

---

## 1. MAVLink ICD verification

### 1.1 Dialect / framing

| Check | Expected | Observed | Result |
|-------|----------|----------|--------|
| Dialect | common.xml MAVLink v1 | ICD + V1_PLAN §5 | PASS |
| Custom dialect / binary | Forbidden | None in ICD/headers | PASS |
| Video on C2 | Forbidden | Explicit reject list | PASS |

### 1.2 Frozen uplink names

| Name | Encoding | SM / effect | ICD | `mavlink_v1_cmds.h` | Result |
|------|----------|-------------|---------------------|---------------------|--------|
| WORK | USER_1 param1=**1** | Enter/continue mission | Y | `AP_UPLINK_WORK` | PASS |
| ABORT | USER_1 param1=**2** | End engagement ≠ FTS | Y | `AP_UPLINK_ABORT` | PASS |
| RTB | NAV_RETURN_TO_LAUNCH (**20**) | Return / recovery | Y | `AP_MAV_CMD_NAV_RETURN_TO_LAUNCH` | PASS |
| FTS | DO_FLIGHTTERMINATION (**185**) | Non-explosive; commanded-only | Y | `AP_MAV_CMD_DO_FLIGHTTERMINATION` | PASS |
| TARGET_DESTROYED | USER_1 param1=**3** | Clear engagement → **RTB always** (not LAND_SOFT) | Y | `AP_UPLINK_TARGET_DESTROYED` | PASS |
| MISS/REATTACK | USER_1 param1=**4** | Clear CLOSE; allow WORK / re-cue | Y | `AP_UPLINK_MISS_REATTACK` | PASS |

Wire/UI string for BDA miss path: exactly `MISS/REATTACK`.  
UX labels “MISS” / “REATTACK” may exist but **must both send param1=4** and string `MISS/REATTACK` (header aliases `AP_UPLINK_MISS` / `AP_UPLINK_REATTACK` → same value; names → same string).

**Do not use param1=5** — confirmed in ICD, ground note, gcs_video_bda ICD, bridge README, NOTES.md.

### 1.3 Setpoint + telemetry (thin C2)

| Message | ID | Dir | Rate | V1 |
|---------|----|-----|------|----|
| HEARTBEAT | 0 | Orin↔FC↔GCS | 1 Hz | Y — lost Orin HB **3 s → RTB** (not FTS) |
| SET_POSITION_TARGET_LOCAL_NED | 84 | Orin→FC | 10–20 Hz | Y — heading+climb / commanded miss |
| COMMAND_LONG / ACK | 76 / 77 | as above | event | Y |
| NAMED_VALUE_INT `MISSION_STATE` | 252 | Orin→ | change/2 Hz | Y — BOOT…FTS 0–6 |
| VFR_HUD / BATTERY_STATUS / SYS_STATUS | 74 / 147 / 1 | FC→ | 5/1/1 Hz | Y |
| GPS_RAW_INT | 24 | FC→**GCS only** | 1 Hz | Y — Orin must not guide on GPS |
| STATUSTEXT | 253 | →GCS | event | N |

### 1.4 Explicit rejects (still true)

- HD / any video frames or codec on RFD900 / MAVLink  
- Starlink / phone LTE / raw 5.8 analog as C2  
- Custom MAVLink dialect  
- Auto-FTS on lost-link  
- TARGET_DESTROYED → LAND_SOFT  
- param1=5 for REATTACK

---

## 2. TELEM map — FC install (N250 / Pixhawk 6X / Cube class)

Repo freezes FC as **PX4 Pixhawk 6X / Cube Orange+ class**. Literal **`N250` does not appear** in `/workspace/actprove-drone`. Audit treats N250 install as that class unless hardware renames it.

| Port | Role | Baud / air | Install notes |
|------|------|------------|---------------|
| **TELEM1** | RFD900x-class C2 (900 MHz) | **57600** UART; **~64 kbps** air | Wingtip-only antenna. Primary GCS↔FC. Separate BEC preferred (do not starve FC 5 V). No flow control. |
| **TELEM2** | Orin UART MAVLink | **57600** (match `SERIALn`) | Onboard peer; Orin `compid=191`. USB must **not** be primary Orin↔FC. |
| Video RF | Near-field digital 1.2 or 2.4 GHz | separate | Rail/tower Rx; air Tx from Orin/cam. **Not** TELEM*, **not** MAVLink. |

```
[GCS] --RF 900 MHz-- [RFD900 air] --UART-- FC TELEM1
                                           FC TELEM2 --UART-- [Orin]
[rail/tower Rx] --RF 1.2/2.4-- [air Tx] <-- cam/Orin (not MAVLink)
```

**IDs:** vehicle `sysid=1`; Orin `compid=191`; GCS `sysid=255`.

**Bring-up refs:** `docs/bringup/mavlink_telem2_check.md`, `docs/bringup/orin_first_boot.md`, `onboard/mavlink_bridge/`.

**OPEN for N250:** If N250 is a named airframe/FC SKU distinct from 6X/Cube, hardware must confirm TELEM1/TELEM2 silk labels match this map before harness cut.

---

## 3. RFD900 + near-field video

| Item | Qty | Interface | Status | Audit |
|------|-----|-----------|--------|-------|
| RFD900x-class modem pair 900 MHz | 1 pair | FC TELEM1; 57600; ~64 kbps; wingtip ant | **BUY** | PASS — C2 only; no HD |
| Near-field digital video Tx | 1 | Separate RF 1.2 or 2.4 GHz | **BUY V1** | PASS role; **SKU TBD** |
| Near-field digital video Rx | 1 | Rail/tower | **BUY V1** | PASS role; **SKU TBD** |
| Orin↔FC UART harness | 1 | TELEM2 | **BUY** | PASS |
| WiFi / Starlink / LTE / 5.8 analog as C2 | — | — | **REJECT** | PASS (rejected) |

Video SKU/band still open after cam output (Integration/hardware). Does not block MAVLink freeze.

---

## 4. Virtual tests (doc/header consistency)

Executed 2026-09-16 on shared box tree (no RF).

| ID | Test | Result |
|----|------|--------|
| VT-01 | Dialect is common-only; no custom XML in ICD | **PASS** |
| VT-02 | WORK=1, ABORT=2 in ICD ↔ `mavlink_v1_cmds.h` | **PASS** |
| VT-03 | RTB=cmd 20, FTS=cmd 185 | **PASS** |
| VT-04 | TARGET_DESTROYED=3; MISS/REATTACK=4; wire string exact | **PASS** |
| VT-05 | No allocated param1=5 (aliases map to 4) | **PASS** |
| VT-06 | Lost-link timer = 3 s → RTB | **PASS** |
| VT-07 | TELEM baud 57600; air ~64 kbps; TELEM1=RFD; TELEM2=Orin | **PASS** |
| VT-08 | Video explicitly not on MAVLink/C2 | **PASS** |
| VT-09 | TARGET_DESTROYED path forbids LAND_SOFT | **PASS** |
| VT-10 | Ground BDA note + gcs_video_bda ICD agree with cmds.h | **PASS** |
| VT-11 | `send_miss()` / `send_reattack()` both encode param1=4 | **PASS** (bridge README + header aliases) |
| VT-12 | N250 named pinout present in repo | **OPEN** — not found; class map PASS |

**Score:** 11 PASS / 0 FAIL / 1 OPEN.

### Virtual SM / C2 sequence checks (logic only)

| Seq | Steps | Expected | Result |
|-----|-------|----------|--------|
| S1 | Lost Orin HEARTBEAT ≥3 s | FC RTB; not FTS | PASS (ICD) |
| S2 | GCS `TARGET_DESTROYED` | SM → RTB always | PASS |
| S3 | GCS `MISS/REATTACK` | Clear CLOSE; allow WORK | PASS |
| S4 | GCS ABORT | Engagement end; ≠ FTS | PASS |
| S5 | Inject HD frame on TELEM1 | Reject / not in dict | PASS (forbid) |
| S6 | param1=5 on USER_1 | Not defined; must not be handled as REATTACK | PASS (forbidden) |

---

## 5. File stamp checklist

| Path | Role | Status |
|------|------|--------|
| `docs/icd/mavlink_v1.md` | Authoritative ICD | FROZEN — matches audit |
| `interfaces/mavlink/mavlink_v1_cmds.h` | C constants | FROZEN — 3/4 + aliases→4 |
| `interfaces/mavlink/telem_map.h` | TELEM roles/baud | FROZEN |
| `interfaces/mavlink/mission_state.h` | BOOT…FTS | Present |
| `interfaces/mavlink/message_table.md` | Quick ref | Aligned |
| `docs/notes/GROUND_EO_CUE_AND_BDA.md` | BDA + ground EO | Aligned (no p1=5) |
| `docs/icd/gcs_video_bda.md` | GCS BDA contract | Aligned |
| `V1_PLAN.md` §5 | Plan freeze | Aligned (BDA detail in ICD/notes) |

---

## 6. Actions / owners

| Action | Owner | Priority |
|--------|-------|----------|
| Confirm N250 ≡ Pixhawk 6X / Cube Orange+ TELEM silk | hardware / airframe-iface | Medium — before harness fab |
| Pick near-field video Tx/Rx SKU + 1.2 vs 2.4 GHz | comms + hardware | Medium — after cam output known |
| Keep safety_gates / bridge on exact strings `TARGET_DESTROYED`, `MISS/REATTACK` | safety + system stack | Done if matching cmds.h |
| Sync this audit + ICD to user Downloads | pm | On ack |

---

## 7. Sign-off

**COMMS:** Audit complete — MAVLink + BDA 3/4 + TELEM/RFD900 **PASS**; video SKU and N250 identity **OPEN**.  
No change required to frozen wire map.
