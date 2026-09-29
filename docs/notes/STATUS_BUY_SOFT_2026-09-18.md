# ACTPROVE status — BUY NOW + software remaining

**Date:** 2026-09-18 (CT)

## 1) BUY NOW — first-test mission

**Mission rail:** `MANUAL climb → TEST → SEARCH → red box / LOCK_HOLD → operator Kill → CLOSE → LAND/RTL`.

This is the Mass A profile: the companion island must weigh **≤680 g** before flight. `Cube Orange+`, Here3+, and ELRS are the FC/safety island and are outside that 680 g companion budget.

### Tier A / required COTS

- **RFD900x-US FCC bundle:** two modems, antennas, and FTDI USB; air modem to Cube TELEM1, ground modem to GCS.
- **Cube Orange+**, carrier board, and power module.
- **Here3+** dedicated Cube GPS (do not share the Orin F9P/GNSS path).
- **ELRS radio path:** TX16S MKII ELRS, or existing radio plus Ranger module, and at least one air RX; pilot reclaim/abort is always live.
- **TELEM2 UART:** Holybro 1186, or Digi-Key GHR-06V-S plus SSHL-002T-P0.2 contacts. Keep Cube 5 V unconnected.
- **Cube TELEM1↔RFD cable:** JST-GH 6→6, preferably power-removed if not already in the RFD bundle.

### Companion island (weigh assembled)

- **Jetson Orin NX 16 GB**.
- **Bootable light carrier** (dev/Seeed/Auvidea-class is acceptable for this test; not Forecr).
- **Cooler/heatsink**.
- Small **NVMe** for logs/weights.
- TELEM/UART cables, short RFD air antennas, and an interim 3D-printed/clamp mount plus cables.
- **Stamped interim EO:** Basler **daA1920-160uc S-Mount, order #108233** (15 g) + Edmund Optics **#54-854**, 12 mm M12 lens (6 g), approximately 30° horizontal FOV; camera+lens about **21 g**.

The documented companion estimate is approximately **270–550 g**, but the assembled island still must be weighed and pass ≤680 g.

### Target, power, and range

- Expendable **test drone** with known size and photos for the `test_drone` class; bright/unique markings are useful.
- Batteries/packs for both craft, plus the interim bench/air power and harness needed to bring up Cube, radios, and Orin.
- Closed range and geofence plan; prove ELRS abort before any CLOSE.
- Laptop with ACTPROVE GCS or Mission Planner if not already available.

### Explicit DO NOT BUY for this first-test gate

- **Forecr** DSBOARD/FAE carrier or GMSL add-on PO.
- **FIM-2410** (thermal/near-field SA is cut from this gate).
- **Basler ace + 40 mm** / a2A Cam A + 40 mm lock optic.
- **Sync mezzanine** / PPS-FSYNC-DRDY NRE PCB.
- **Thermal** (FLIR/thermal payload).

Also defer flight CNC EO/IMU hardware; use the interim mount. Cloud/Vercel/Supabase is never C2.

## 2) Software remaining work

### Required behavior (frozen first-test rail)

1. **TEST before Engage:** manual climb, enter TEST on a separate TEST channel (default CH8), and run the seconds-scale bounded control sample. TEST must not Lock or CLOSE.
2. **TEST card must pass:** ELRS reclaim/abort, failsafe/RTL-or-hold, Orin↔Cube heartbeat, EO frames, bounded authority limits, and TEST abort-to-MANUAL.
3. **SEARCH:** only after TEST PASS and CH7 rising edge; detect airborne targets and prefer the `test_drone` class. Reject people, ground clutter, COCO, Shahed labels, and DEMO as Lock candidates.
4. **Red box:** stable fixed candidate becomes `LOCK_HOLD`; it is only proposed Lock, not authorization. Move to `WAIT_AUTHORIZE` and show local GCS **Kill / Збити** and Abort.
5. **Kill gate:** only local GCS/companion Kill sets `kill_authorized`; CH7 is not Kill. CLOSE/GUIDED is forbidden until that flag is true.
6. **Terminal:** after contact or the agreed soft-kill distance (≤15 m in the minimal-stack stamp; define/test the exact range card), LAND when `TEST_RECOVER` is set, otherwise RTL. ELRS/RFD abort, timeout, LAND/RTL clear authorization and override CLOSE.

### Remaining implementation and integration

- **Complete Orin MAVLink parsing:** host `EngageDecoder` smoke is PASS, but `Orin mavlink_parse_char` / live `RC_CHANNELS` handling is still TODO. Wire CH8 TEST, `test_pass`, CH7 Engage, `engage_armed`, and the `WAIT_AUTHORIZE`/`kill_authorized` state machine.
- **Bench integration:** with Cube serial RC_CHANNELS, UART, and ELRS on the desk, prove TEST→PASS→CH7→SEARCH and abort. The Integration ETA note says this is **this week** once UART+ELRS are available; GCS live mirror follows the first bench PASS.
- **Implement/validate the Kill wire:** local GCS Kill over RFD or companion UI, preferably `COMMAND_LONG` `MAV_CMD_USER_1`, `param1=6`; verify red box alone, CH7, DEMO, cloud, timeout, and abort cannot cause CLOSE.
- **Make RFD live:** replace the dry-run `gcs_rfd_bridge` with HEARTBEAT, cue/WORK, telem, and ABORT; connect Mission Planner or ACTPROVE GCS. Configure Cube TELEM1=RFD, TELEM2=Orin, ELRS RCIN, and failsafe behavior.
- **Enable FS-03 on air:** companion-heartbeat loss must produce the documented RTL/abort behavior.
- **Finish Orin↔Cube live TrackMsg path:** UART heartbeat, camera frames, tracking message, GUIDED setpoints, and LAND/RTL must be observable in logs. Do not steer on estimated range alone.
- **Tracking/Nav:** implement/validate `test_drone` hold/coast and the `TEST_GEOMETRY` GUIDED CLOSE path, only after operator authorization; log JPEG rate, track, MAVLink, and the TEST card result.
- **TEST scoring and range card:** update fly/defeat criteria for contact or agreed ≤X m soft-kill, closed-range/geofence rules, ELRS-only residual risk if no FTS, and the staged flights: RC-only smoke; RFD+GUIDED with no target; static ground Lock; then air versus expendable test drone.

### Perception remaining

- The actual perception inbox is empty: `vision/data/inbox/test_drone/` contains no images, and `vision/data/inbox/negative/` is empty.
- Collect at least **200–500** images of the actual expendable test UAV across altitude, aspect, and background; collect negatives (sky, people, ground clutter, birds).
- Label tight boxes (Label Studio/`label_batch`), train/export the `test_drone`-only model to Orin, and enforce person/COCO rejection. Until photos/labels arrive, an airborne heuristic may draw a DEMO/candidate red box, but field Lock remains blocked.
- Bring up the interim Orin EO path before onboard detection smoke. Mac GoPro remains a local DEMO only, not flight Lock.

### Integration status / ETA notes

- Integration ICDs and ownership are stamped, but the live Orin RC parser, bench RC/UART proof, live GCS mirror, RFD bridge, TrackMsg path, and fly/defeat validation remain.
- The accepted 2-week minimum is RFD HEARTBEAT + `ORBIT_CUE_SET`/WORK on `TEST_GEOMETRY` plus Cube↔Orin UART; the fly/defeat update additionally requires `test_drone` Lock→authorized CLOSE.
- No separate Integration ETA document was present beyond `docs/integration/ENGAGE_ARMED_WIRE.md`: host decode **DONE**; Cube serial bench **this week** when UART+ELRS are on desk; GCS mirror **after first bench PASS**.
- Do not wait for Forecr, sync mezzanine, PPS/FSYNC Lock PASS, thermal, or CNC mounts for this first-test gate.

### Source documents used

- `docs/hardware/MANUAL_ENGAGE_MIN_STACK_CALC_2026-09-18.md`
- `docs/hardware/BUY_THIS_WEEK_US_2026-09-18.md`
- `docs/hardware/MANUAL_ENGAGE_15LB_MASS.md`
- `BUY_LIST.md`
- `docs/notes/OPERATOR_AUTHORIZE_KILL_2026-09-18.md` and perception ACK
- `docs/notes/ONBOARD_TEST_PREENGAGE_2026-09-18.md` and perception ACK
- `docs/notes/TWO_WEEK_FLY_KILL_TEST_DRONE_2026-09-18.md` and perception ACK
- Integration ACK/ICD and ETA notes referenced by those docs.
