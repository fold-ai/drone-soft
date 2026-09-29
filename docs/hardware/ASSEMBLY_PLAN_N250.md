# Assembly & bring-up plan — N250 + V1 EO interceptor stack

**Goal:** Install full avionics into given airframe+N250; prove G1 bench → G2 iron bird → G-RANGE-NR (ground EO) → G3.

English engineering doc. Ops UI language: English.

---

## Phase 0 — Paper (this week)
1. Freeze EO-only BOM (this stack). Thermal stays V1.1.
2. Send Forecr FAE (HTE / PPS / FSYNC / mass).
3. User: pack V + battery SKU; name FTS holder.
4. Tracking: stamp Basler SKU (1920-168mgm vs 2048-114mgc) + FoV.
5. Print harness drawings: TELEM2 UART, SPI0 ADIS, FAKRA nose, PPS/FSYNC mezz.
6. Mass/CG spreadsheet: Orin island + FC + EO + radios vs N250 empty CG.

**Exit:** Integration checklist started; no carrier PO yet.

---

## Phase 1 — Buy & kitting (after blockers clear)
**Buy now (not blocked):** Orin NX 16GB, FC PX4 class, RFD900x pair, NVMe, Orin cooler, ADIS16470, ZED-F9P+ant, baro, UART harness, FAKRA jump, GMSL harness, EO+IMU mount class, sync mezz NRE start.

**Buy when PASS:** Forecr carrier+GMSL addon, isolated PSU PNs, Basler+lens (if FoV stamped), near-field video SKU.

**Kit:** Label bags A–F per HARDWARE_STACK_N250.md. Flash Orin base image; PX4 parameter set stub; GCS laptop with `gcs-ui` + QGC.

---

## Phase 2 — Bench island (no airframe) = G1 prep
1. Assemble Orin on carrier + cooler + NVMe on bench jig.
2. Wire FC TELEM2 ↔ Orin UART; TELEM1 ↔ RFD900 (short).
3. Bring up MAVLink HEARTBEAT both ways; run `mavlink_bridge` smoke.
4. Mount ADIS on SPI0; verify DRDY→HTE **or** document soft-fail until mezz.
5. GNSS PPS into mezz; FSYNC to EO (or trigger box).
6. Basler on GMSL; 30/60 fps; DetectionMsg latency check.
7. Run: safety_gates tests, navigation_smoke, tracking handoff unit, perception detect on val images.
8. GCS demo: WORK → Lock → CLOSE → Mark destroyed → RTB; Mark miss → re-WORK; TEST_RECOVER soft-land path.

**Exit G1 software:** all smokes green on bench logs.  
**Exit G1 hardware:** EO frames + MAVLink + IMU samples time-stamped same domain (or waiver list).

---

## Phase 3 — Install into N250 airframe (iron bird / G2)
1. **Power first:** pack → island DC-DC; verify isol rails; jet ECU on its own supply — scope for noise on EO rail.
2. Mount FC on isolators; Orin tray aft of CG plan; route loom away from hot N250 section.
3. Nose: EO+IMU bracket on **airframe hardpoints** (not turbine case); FAKRA through bulkhead.
4. GNSS ant on top; RFD900 ant **wingtip**; video Tx as ICD.
5. Weight & balance; control surface / jet run-up with SAFE FC (manual/hold).
6. Iron-bird: same software chain as G1 with vibration running (jet idle if safe).

**Exit G2:** Integration PASS stamp on sync+EMI+mass; test card G2 signed; FTS holder present.

---

## Phase 4 — Range test no radar (G-RANGE-NR-V1)
Per `test/g_range_no_radar/TEST_CARD.md`:
1. Ground EO USB into GCS (`cam_id=2`).
2. Interceptor on rail/trail; nose may not see target yet.
3. Operator aims Ground EO → Manual Lock (image box).
4. WORK / launch → onboard acquire when in FoV → CLOSE.
5. Operator **Mark destroyed** → RTB **or** **Mark miss** → re-WORK.
6. Optional TEST_RECOVER soft-land only on auto post-CLOSE miss path (not after BDA destroy).

**Exit:** card PASS/FAIL + `bda.jsonl` logs.

---

## Phase 5 — Flight G3 (when G2 green)
Commanded miss at ~100 km/h target class; class-gated Lock; lost-link → RTB; FTS commanded-only. No combat intercept scope creep.

---

## Wiring cheat-sheet
```
Nose Basler GMSL2 ──FAKRA──► Forecr GMSL addon ──► Orin
ADIS SPI+DRDY ──SPI0/mezz──► Orin
GNSS PPS ──► mezz ──FSYNC──► Basler
Orin UART ──► FC TELEM2
RFD900 ──► FC TELEM1
Near-field video ──► separate RF (Orin/cam out)
Ground USB cam ──► GCS laptop only (range)
```

## Safety install rules
- Never share EO 12 V with jet pump/ECU.
- Never hard-bolt EO/IMU to N250 case.
- Never auto-FTS on lost-link.
- Carrier PO only after Integration PPS/trigger PASS.

## Virtual testing (software) — parallel now
Channels run audits under `docs/audit/*_AUDIT.md` + `TEST_VIRTUAL_RUN.md`. PM consolidates when all report.
