# System Stack Audit + Virtual Testing (V1)

**Channel:** System stack  
**Date / stamp:** 2026-09-16 18:48 CDT (America/Chicago)  
**Repo root:** `/workspace/actprove-drone/`  
**Language:** English  
**Trigger:** PM-directed full System stack audit + virtual tests  

---

## 1. Scope

### In scope (owned)

| Path | Role |
|------|------|
| `onboard/mavlink_bridge/` | Orin ↔ FC TELEM2 UART scaffold + BDA uplink API stubs |
| `onboard/logging/` | Black-box writer + `logger_smoke` + schema |
| `onboard/bringup/` | Start-order scripts, launch sketch, systemd units |
| `onboard/time_sync/` | PPS clock / ingest scaffold |
| `onboard/safety_gates/` tests | **Dependency only** — policy owned by Safety; run as stack glue |
| `gcs-ui/` | GCS glue: Nose\|Ground video toggle, BDA buttons, mission sim |
| `docs/icd/` (owned contracts) | `mavlink_v1.md`, `latency_budget.md`, `time_sync.md`, `gcs_video_bda.md`, track `measurement_epoch` |
| `tools/latency_probe` | Cam→Orin / E2E latency gates |
| `tools/log_replay` | Offline black-box ICD field validation |
| `tools/extrinsics_calib` | Lever-arm YAML schema check (Nav owns estimation) |

### Out of scope (one-line)

- **Vision / Perception** deep audit (USB ingest implementation, YOLO, Argus) — out of scope.  
- **Tracking / Navigation** deep audit — out of scope (except ICD cross-checks).  
- **Hardware BOM / airframe** — out of scope (N250 jet given; we own avionics/sensors/PSU/C2 *design glue* only).  
- **Class Lock IDs** (`shahed_136` / `geran_2` / `gerbera`) — System stack code does **not** touch class IDs; N/A for this audit.

### V1 freezes reflected

- EO-only V1 (thermal / night vision deferred; `cam_id=1` reserved, not activated).  
- Airframe N250 given; stack owns avionics/sensors/PSU/C2 design glue.  
- Ground EO cue + BDA: `TARGET_DESTROYED` param1=**3** → RTB always; `MISS/REATTACK` param1=**4**; `TEST_RECOVER` `LAND_SOFT` separate; BDA destroy never `LAND_SOFT`.

---

## 2. Executive summary

| Verdict | **PARTIAL** |
|---------|-------------|
| Host smokes / builds that can run | **All executed runnable items exited 0** |
| Wire-level / HW-proven | **Not proven** (UART TELEM2, PPS/GPIO, USB Ground EO absent on this host) |
| MAVLink pack path | **Scaffold only** — `bridge.cpp` does not emit mavlink bytes |
| GCS integration | **Build + mission-sim BDA OK**; no live C2 / `cam_id=2` DetectionMsg from UI |
| ICD constants TELEM / BDA / epoch / cam_id | **Consistent** in owned headers + GCS sim; minor ICD doc gap on TrackMsg `cam_id` |

**Overall: PARTIAL** — virtual/host scaffolding and ICD freeze alignment are healthy; production wire path and real time-domain discipline remain open.

---

## 3. Test matrix

| Name | Command | Result | Notes |
|------|---------|--------|-------|
| onboard CMake build | `cmake --build onboard/build -j` | **PASS** (exit 0) | Targets: `mavlink_bridge`, `mavlink_bridge_smoke`, `logging`, `logger_smoke`, `time_sync`, `safety_gates` |
| `mavlink_bridge_smoke` | `./onboard/build/mavlink_bridge_smoke` | **PASS*** (exit 0) | `open=0` — `/dev/ttyTHS1` missing (expected). Soft null-sink scaffold; HEARTBEAT×3 + `MISSION_STATE=BOOT` logged. *Not a wire proof. |
| `logger_smoke` | `./onboard/build/logger_smoke` | **PASS** (exit 0) | Wrote `/tmp/actprove_blackbox/<run>/` with `meta.json`, `camera_ts.jsonl`, `tracks.jsonl`, `mission_state.jsonl` |
| `test_mission_sm` (Safety dep.) | `./onboard/safety_gates/build/test_mission_sm` | **PASS** (exit 0) | ALL TESTS PASSED — BDA param1 3/4, TARGET_DESTROYED→RTB never LAND_SOFT, MISS/REATTACK→ABORT, lost-link 3s→RTB. **Not claiming Safety policy ownership.** |
| `latency_probe --demo` | `python3 tools/latency_probe/latency_probe.py --demo` | **PASS** (exit 0) | Synthetic cam→Orin p95=33.6 ms; E2E p95=68 ms within budgets |
| `latency_probe` on smoke logs | `... --camera-jsonl <run>/camera_ts.jsonl --track-jsonl <run>/tracks.jsonl` | **PASS** (exit 0) | n=1: cam 25 ms; E2E 60 ms |
| `log_replay` on smoke run | `python3 tools/log_replay/log_replay.py <run_dir>` | **PASS** (exit 0) | `PASS ...: ICD fields present` |
| `log_replay --demo` | N/A | **SKIP** | Flag not implemented; exercised via real `logger_smoke` run_dir instead |
| `extrinsics_calib` example YAML | `python3 tools/extrinsics_calib/extrinsics_calib.py tools/extrinsics_calib/lever_arm.example.yaml` | **PASS** (exit 0) | Schema OK; lever_arm=(0.05,0,0.02) m |
| `extrinsics_calib --demo` | N/A | **SKIP** | Flag not implemented; example YAML used |
| `bringup/start_order.sh` | `onboard/bringup/scripts/start_order.sh` | **PASS** (exit 0) | Full 1–5 sequence; **PPS ready is scaffold** (`: > /tmp/actprove/pps_ready`); mavlink open fails soft |
| `bringup/stop_all.sh` | `onboard/bringup/scripts/stop_all.sh` | **PASS** (exit 0) | `bringup-stop ... done` |
| `gcs-ui` production build | `cd gcs-ui && npm run build` | **PASS** (exit 0) | `tsc -b && vite build` OK |
| `gcs-ui` lint | `cd gcs-ui && npm run lint` | **PASS** (exit 0) | 2× oxlint react(refs) warnings in `TacticalMap.tsx` — non-blocking |
| `time_sync` smoke / unit | *(no CMake executable)* | **SKIP** | Library only; no host smoke target |
| Real TELEM2 UART | — | **SKIP** | No `/dev/ttyTHS1` / FC on this host |
| Real PPS / `/dev/pps0` | — | **SKIP** | No PPS hardware; ingest `simulate=true` |
| Live USB Ground EO | — | **SKIP** | No `/dev/video0` path exercised from System stack |

### Counts

| Passed | Failed | Skipped |
|-------:|-------:|--------:|
| **12** | **0** | **5** |

(Passed includes Safety dependency smoke run as glue. Skipped = missing `--demo` flags + no time_sync smoke + 3 HW-absent virtual-test gaps.)

---

## 4. ICD / contract checks

| Contract | Expected (V1 freeze) | Observed in owned artifacts | Status |
|----------|----------------------|-----------------------------|--------|
| **TELEM1** | RFD900x GCS C2 @ 57600 / ~64 kbps | `interfaces/mavlink/telem_map.h` `AP_TELEM1_ROLE`; ICD `mavlink_v1.md`; bridge README | **PASS** |
| **TELEM2** | Orin UART MAVLink @ **57600**, `compid=191` | `BridgeConfig` default baud 57600, port `/dev/ttyTHS1`; bringup `ACTPROVE_TELEM2_BAUD` | **PASS** (docs/code); open unproven on host |
| **BDA TARGET_DESTROYED** | USER_1 **param1=3** → RTB always; never LAND_SOFT | `AP_UPLINK_TARGET_DESTROYED 3u`; bridge `send_target_destroyed()`; GCS sim → RTB + toast; Safety tests PASS | **PASS** |
| **BDA MISS/REATTACK** | USER_1 **param1=4**; no param1=5 | `AP_UPLINK_MISS_REATTACK 4u`; `send_miss`/`send_reattack` same wire; GCS miss → ABORT; Safety tests PASS | **PASS** |
| **LAND_SOFT vs BDA** | `TEST_RECOVER` LAND_SOFT only auto post-CLOSE miss; BDA destroy never LAND_SOFT | GCS `useMissionSim` freeze comment + behavior; Safety `TARGET_DESTROYED never LAND_SOFT` | **PASS** (sim + Safety dep.) |
| **Video source Nose \| Ground** | UI toggle; Ground = GCS laptop USB | GCS `CameraFeed` Nose EO / Ground EO toggle | **PASS** (UI); live capture not wired |
| **cam_id GroundEo=2** | Nose=0, THERMAL=1 (V1.1), GroundEo=**2** | `docs/icd/gcs_video_bda.md`, logging schema, detection ICD; GCS README documents 2 | **PASS** in ICD/schema; **GCS UI does not emit numeric `cam_id`** (Perception owns ingest) |
| **measurement_epoch** | PPS mid-exposure; soft `now()` forbidden; coast keeps last real | `time_sync::FrameTimestamp`, logging `CameraTsRecord`/`TrackRecord`, latency_probe keys, log_replay required keys | **PASS** |
| **TrackMsg cam_id** | Should align with GroundEo=2 | `docs/icd/track_msg.md` still lists EO=0 / THERMAL=1 only | **GAP** (doc drift) |
| **Video on MAVLink** | Forbidden | Explicit in ICD + bridge comments | **PASS** |
| **Class Lock IDs** | Mention only if stack touches | No class ID handling in owned System stack code | **N/A** |

---

## 5. Gaps & blockers (ranked)

### P0 — blockers for Orin / range-test wire

1. **`mavlink_bridge` is a soft stub** — `bridge.cpp` comments show `mavlink_msg_*_pack` but **no bytes are written** to the UART even if `open()` succeeds. Host smoke always soft-fails open and still exits 0. Cannot prove HEARTBEAT / COMMAND_LONG BDA on TELEM2 until common dialect pack + real UART.  
2. **`time_sync` has no smoke target + bringup fakes PPS** — `start_order.sh` touches `/tmp/actprove/pps_ready` without calling `PpsIngest` / `PpsClock::ready_for_cameras()`. ICD rule “cameras must not start until PPS ready” is **not enforced** by a real gate on this scaffold. Forecr status (ICD): SPI0 PASS · DRDY→HTE **FAIL** · PPS/FSYNC pins unnamed.  
3. **GCS ↔ live C2 / Ground EO not integrated** — mission sim correctly encodes BDA→RTB / MISS semantics, but UI does not send USER_1 param1=3/4 over TELEM1/bridge, and does not publish `DetectionMsg` with `cam_id=2`. Video toggle is UI+cue routing only (matches ICD note).

### P1 — quality / completeness

4. **`logger_smoke` does not exercise `bda.jsonl` / `gcs_events.jsonl`** though schema + `BlackboxWriter::write_bda` exist — black-box BDA stream unproven in smoke.  
5. **`track_msg.md` cam_id table omits GroundEo=2** while `gcs_video_bda.md` / detection ICD / logging schema include it — ICD drift risk for Tracking consumers.  
6. **Tools CLI inconsistency** — `latency_probe` has `--demo`; `log_replay` / `extrinsics_calib` do not (audit used equivalent real inputs).  
7. **`start_order.sh` ignores `--help` / `--dry-run`** — always runs full sequence (including building-side smokes).  
8. **GCS lint warnings** — `TacticalMap.tsx` react refs-during-render (non-blocking).

### P2 — host virtual-test limitations (expected)

9. No real UART TELEM2, GNSS PPS, or USB camera on this audit host.  
10. Launch XML / systemd units are placeholders — not exercised under real systemd on Orin.  
11. Extrinsics tool validates schema only — Navigation owns estimation (by design).

---

## 6. Recommendations for N250 EO-only V1

1. **Wire mavlink c_library_v2 common dialect** into `mavlink_bridge`: real pack for HEARTBEAT, NAMED_VALUE_INT(`MISSION_STATE`), COMMAND_LONG USER_1 (1–4), SET_POSITION_TARGET_LOCAL_NED; fail smoke if `open()` false unless `--allow-null-sink`.  
2. **Add `time_sync_smoke`** that starts `PpsIngest(simulate=true)`, asserts `ready_for_cameras()`, and make `start_order.sh` call it (remove bare `touch` gate). On Orin, fail closed if `/dev/pps0` / named GPIO missing.  
3. **Extend `logger_smoke`** to write one `bda.jsonl` row (`TARGET_DESTROYED`, `video_source=ground`) and optional `gcs_events` video_source_changed; keep `log_replay` REQUIRED/OPTIONAL keys aligned.  
4. **GCS next glue step:** map Mark destroyed / Mark miss buttons to documented uplink strings + param1; stamp Ground EO cue path with `cam_id=2` when Perception ingest is live; keep thermal/`cam_id=1` dark for V1.  
5. **Patch `docs/icd/track_msg.md`** cam_id row to NoseEO=0 / THERMAL=1 (V1.1) / **GroundEo=2**.  
6. **Add `--demo` to `log_replay` and `extrinsics_calib`** (or document “use example/smoke run” in READMEs) for one-command CI.  
7. Keep **Safety** as dependency: continue running `test_mission_sm` in System stack CI gates without claiming policy ownership.

---

## 7. Artifact references

| Artifact | Path |
|----------|------|
| This report | `docs/audit/SYSTEM_STACK_AUDIT.md` |
| MAVLink ICD | `docs/icd/mavlink_v1.md` |
| GCS video + BDA | `docs/icd/gcs_video_bda.md` |
| Time sync | `docs/icd/time_sync.md` |
| Latency | `docs/icd/latency_budget.md` |
| Track epoch | `docs/icd/track_msg.md` |
| Constants | `interfaces/mavlink/{telem_map,mavlink_v1_cmds,mission_state}.h` |
| Black-box schema | `onboard/logging/schema/blackbox_streams.json` |
| TELEM2 bringup check | `docs/bringup/mavlink_telem2_check.md` |

---

## 8. Sign-off

**System stack virtual audit complete on host.**  
Runnable owned smokes: **executed**. Fabricated passes: **none**.  
**Verdict: PARTIAL** — ICD freezes for TELEM1/2, BDA 3/4, measurement_epoch, Nose|Ground / GroundEo=2 are reflected in owned docs and scaffolds; production UART/PPS/GCS live paths remain open for Orin bring-up.
