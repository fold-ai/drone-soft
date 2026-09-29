# Integration Audit — N250 jet + EO-only payload island

**Doc:** `docs/audit/INTEGRATION_AUDIT.md`  
**Owner:** Integration  
**Stamp:** 2026-09-16 18:50 CT  
**Authority:** `/workspace/bom/BOM-v0.1.md`, Nose ICD V1 (`/workspace/airframe-iface/NOSE_ICD_V1.md`), `V1_PLAN.md` §3, channel stamps (hardware, Navigation, airframe-iface, Tracking, PM, System stack, comms)  
**Scope:** Physical/logical interfaces + buy-gates for V1. **Not** algorithms, seekers, warheads, or a second compute box.

**Platform assumption:** N250-class small turbojet interceptor airframe (frozen span/sweep/fuel mass per airframe-iface). Payload = single Orin NX compute island + nose EO kit.

**V1 sensor descope:** **EO-only** (Basler GS GMSL2). FLIR ADK / thermal **DEFER V1.1**.

---

## 1. Executive verdict

| Area | Verdict | Notes |
|------|---------|-------|
| Sync domain (architecture) | **PASS design / FAIL buy** | GNSS PPS → mezz → cam FSYNC + ADIS DRDY HW-ts is locked; Forecr lacks published HTE GPIO |
| Mass (island class) | **CONDITIONAL PASS** | Known ~113 g (SOM+board); addon+cooler+PSU UNKNOWN; ≪ 0.5–0.8 kg target if addon stays light |
| EMI / jet coupling | **PASS rules / HOLD evidence** | Isolation + GMSL + routing rules stamped; no measured EMI campaign yet |
| FAKRA nose | **PASS (FIRM ACCEPT)** | Mini-FAKRA (island) → FAKRA bulkhead → nose EO |
| PPS / HTE blockers | **FAIL — blocks Integration PASS** | FAE must name HTE GPIO + PPS/FSYNC pins; draft awaiting send |
| **Integration PASS / carrier PO** | **NO** | Do not buy Forecr carrier until HTE+PPS/FSYNC+addon mass close |

---

## 2. Interface map (V1 EO-only)

```
F9P TIMEPULSE (PPS)
        │
        ▼
Sync mezz (custom NRE) ──► Orin time / nvpps
        ├─ Cam FSYNC 30/60 Hz ──► Basler GS (GMSL2) only   ✗ never → ADIS SYNC
        └─ ADIS16470 free-run DRDY ──► HTE/AON GPIO (UNNAMED on Forecr = FAIL)

Nose EO (Basler) ──FAKRA── bulkhead ──Mini-FAKRA──► DSADDON-GMSL-NX ──► Orin NX
Nav IMU (ADIS) ──SPI0 DF11 @ 3.3 V──────────────────► Orin NX
Thermal ADK ── (V1.1) ──────────────────────────────► deferred

Orin NX ──UART MAVLink──► FC TELEM2
FC TELEM1 ──► RFD900x C2 (ant = wingtip only)
Near-field digital video Tx 1.2/2.4 ── separate RF (not C2)
```

Hard rules still in force: cameras → Orin only; USB not primary Orin↔FC; no radar/FPGA/second compute; CSI-2 nose **REJECT**.

---

## 3. Sync domain audit

### 3.1 Locked architecture (Nav + Integration)

1. GNSS **PPS** disciplines Orin time and feeds the sync mezz.  
2. Mezz derives **camera FSYNC** (1–60 Hz) for Basler GMSL deserializer — **not** wired to ADIS SYNC.  
3. **ADIS16470:** free-run + **DRDY HW-timestamped** on the same PPS domain (preferred). Optional later: high-rate IMU SYNC 200–2000 Hz from same timebase.  
4. Soft `clock_gettime` / soft SPI / DRDY polled = **REJECT**.  
5. Driving ADIS SYNC from cam FSYNC @ ≤30 Hz = **REJECT** (undersamples ≥200 Hz IMU).

### 3.2 Carrier path status

| Item | Status |
|------|--------|
| Forecr SPI0 DF11-16 @ 3.3 V (CS0/SCK/MISO/MOSI) | **PASS** — free with GMSL addon (CSI CAM0/CAM2); CS# polarity FAE HOLD |
| Sync mezzanine | **BUY/NRE required** |
| DRDY → HTE/AON GPIO on Forecr | **FAIL** — no published PEE.02-class / HTE-capable 3.3 V GPIO |
| PPS / FSYNC pin names on Forecr | **HOLD** — unnamed pending FAE |
| NGX018 HTE map (P4-3 PEE.02 etc.) | **Obsolete for buy** — NGX018 demoted (no SPI) |
| Nav stamp | **HOLD PASS** (SPI0 ACCEPT; HTE FAIL) |

### 3.3 Rejected sync shortcuts

- Soft SPI bit-bang for ADIS  
- SODIMM SPI scavenger on NGX018 (1.8 V, unpublished, P4 SPI removed)  
- IMU only on PX4 clock without common stamp to GMSL frames  
- Mezz that triggers cameras only and leaves IMU unsynced  
- VN-100 swap “to fix” missing HTE (does not create timestamp path)

**Sync domain conclusion:** Design is sound for N250 EO-only. **Buy is blocked** until Forecr FAE names a real HTE-capable GPIO and PPS/FSYNC pins on one drawing for Nav re-PASS.

---

## 4. Mass audit (payload island)

### 4.1 Gates (airframe-iface)

| Gate | Value |
|------|-------|
| Target island dry (module+carrier+cooling) before harness | ≤ **~0.5–0.8 kg** |
| Soft ceiling | **>1.0 kg** → AUW/CG waiver required |
| Miivii APEX ~1.8 kg / 227×120×65 | **REJECT** |
| Nose kit (window+bracket+EO+IMU) | **Separate** from island dry — do not bury in carrier mass |

### 4.2 Known / unknown stack (Forecr primary)

| Item | Mass | Source |
|------|------|--------|
| Orin NX SOM | **28 g** | NVIDIA forum staff cite (DS silent; final weigh still) |
| DSBOARD-ORNX | **85 g** | Published |
| DSADDON-GMSL-NX | **UNKNOWN** | No grams in datasheet |
| Cooler | **UNKNOWN** | — |
| Sync mezz (est.) | **8–15 g** | Engineering estimate |
| **Known subtotal** | **113 g** | 28+85 |
| Island dry projected | **113 + addon + cooler + mezz + island PSUs** | Should clear 0.5–0.8 kg **if** addon/cooler stay light |

### 4.3 Airframe stamps

- Forecr **85 g class:** **CONDITIONAL ACCEPT** (same class as prior NGX018 accept).  
- Final mass: **HOLD** until addon + cooler numbered and weighed.  
- NGX018 ~127–134 g class remains demoted ALT only (SPI FAIL).

**Mass conclusion:** N250 fuel/span frozen — a 1.8 kg brick is out. Forecr path is mass-credible. **Do not firm BUY** until addon/cooler grams + weigh stamp.

---

## 5. EMI / jet coupling audit (N250)

Turbojet / ESC / ignition environment is the dominant EMI and vibration threat to EO + IMU.

### 5.1 Rules already stamped (PASS as ICD)

| Rule | Stamp | Rationale |
|------|-------|-----------|
| Nose EO = **GMSL2**, not CSI-2 | PASS | Nose run >~0.5 m and dirty EMI; CSI nose REJECT |
| Galvanic-isolated 12 V (EO) and 5/3.3 V rails | PASS (req); PNs HOLD | Not raw motor/ECU/ESC BEC |
| EO+IMU on one rigid bracket + isolators to **airframe hardpoints** | PASS | Not hard-bolted to turbine mount (blade-pass into IMU) |
| Harness ≥ **50 mm** from EGT path; no nose hardware aft of turbine mount plane | PASS (Nose ICD) | Heat + EMI |
| Inlet keep-out ≥ **1.5× inlet lip Ø** | PASS (Nose ICD) | Optical + capture stream |
| RFD900 / 900 MHz ant = **wingtip only** | PASS | Forbidden nose / exhaust / EGT bay |
| Shielded serdes; ESC phase wires away from cam/IMU harness | PASS (System stack) | Conducted/radiated coupling |
| Star-ground / single-point return where practical | PASS (hardware/airframe) | Ground loops |

### 5.2 Open EMI evidence (HOLD)

- No measured conducted/radiated EMI campaign on N250 with Orin+GMSL flying yet.  
- Isolator SKU for turbine blade-pass attenuation still hardware stamp.  
- Pack V unknown → island DC-DC / isolator PNs HOLD (affects rail stiffness under load dump).  
- Cable length nose→island still unmeasured (GMSL default remains correct until measured).

**EMI conclusion:** ICD controls are appropriate for a jet interceptor. **PASS on rules; HOLD on measured proof.** First bring-up should log rail noise and IMU vibration spectra vs throttle.

---

## 6. FAKRA nose audit

| Item | Stamp |
|------|-------|
| Airframe bulkhead connector | **FAKRA** (standard) |
| Island / DSADDON default | **Quad Mini-FAKRA** |
| Required jump | Mini-FAKRA (aft of bulkhead) → **FAKRA bulkhead** → nose EO run |
| Bare MMCX into nose bay | **REJECT** |
| MMCX option on addon | Island-side only, still behind FAKRA bulkhead |
| Pass-throughs | One sealed FAKRA pass-through per EO run; strain relief; no chafe on inlet duct |
| Airframe stamp | **FIRM ACCEPT** |

**FAKRA conclusion:** **PASS.** Builder ICD is clear for N250 nose.

---

## 7. PPS / HTE blockers (critical path)

### 7.1 Blockers that prevent Integration PASS

| ID | Blocker | Owner | Status |
|----|---------|-------|--------|
| B1/B12 | Name **HTE/AON-capable 3.3 V GPIO** for ADIS DRDY | Forecr FAE + hardware | **FAIL** — draft to support@forecr.io awaiting send |
| B1/B12 | Name **PPS in** and **FSYNC out** pins for mezz | Forecr FAE + hardware + Nav | **HOLD** |
| B1 | SPI0 **CS# polarity** | Forecr FAE | HOLD |
| B4 | Addon + cooler **mass** | Forecr docs / weigh | UNKNOWN |
| B3/B11 | Pack Vnom/Vmin/Vmax | Platform / hardware / airframe | HOLD (blocks isolator PNs) |

### 7.2 Re-PASS criteria (Nav + Integration)

Single drawing must show:

1. SPI0 MOSI/MISO/SCK/CS0 (Forecr DF11 — already cited)  
2. Named DRDY → HTE/timestampable capture pin  
3. Named PPS and cam FSYNC pins (EO-only V1; thermal later)  
4. Level-safe 3.3 V to ADIS  
5. Addon mass + cooler mass for airframe final weigh  

Until then: **Nav HOLD PASS**, **Integration no PASS**, **no carrier PO**.

---

## 8. Hard gates rollup (EO-only V1)

| Gate | Result |
|------|--------|
| Cams → Orin only; FC separate PX4 | **PASS** |
| Nose/remote → GMSL2; CSI nose REJECT | **PASS** |
| Orin↔FC UART TELEM2; TELEM1=RFD900; USB not primary | **PASS** |
| No radar / FPGA / second compute | **PASS** |
| Cam + IMU share PPS / HW trigger | **FAIL buy** (arch PASS; HTE unnamed) |
| Isolated cam rails | **PASS req**; PNs HOLD (pack V) |
| Island 15–30 W sust / 60–80 W peak | **PASS** (budget) |
| Global-shutter EO | **PASS** (Basler) |
| Thermal V1 | **DEFER V1.1** |
| Mass class Forecr | **CONDITIONAL PASS** |
| FAKRA nose interconnect | **PASS FIRM** |
| C2 thin / video separate RF | **PASS** |

---

## 9. Reject list (audit-relevant)

- Miivii APEX (~1.8 kg)  
- NGX018 as SPI host / SODIMM scavenger  
- Leopard as SPI host  
- Soft SPI / soft DRDY timestamps  
- ADIS SYNC ← cam FSYNC @ ≤30 Hz  
- CSI-2 nose / long CSI in EMI  
- Bare MMCX into nose bay  
- Non-isolated cam rails on ESC/ECU bus  
- EO+IMU on turbine mount  
- RFD ant in nose/EGT  
- FLIR ADK / thermal as V1 BUY (deferred)  
- Hadron as primary EO (rolling shutter)  
- Seekers / warheads / second compute  

---

## 10. Recommended next actions (Integration)

1. **Send Forecr FAE** (B12) — HTE GPIO, PPS/FSYNC, addon/cooler mass, CS# polarity (EO-primary wording).  
2. On FAE reply: Nav re-PASS drawing → Integration PASS or carrier change.  
3. Weigh island dry once addon/cooler known → airframe final mass stamp.  
4. Get pack V → airframe nominates galvanic isolator PNs.  
5. Keep V1 bring-up to **one** EO GMSL path; leave dual-GMSL capacity for V1.1 thermal.  
6. Plan jet EMI/vib log on first powered N250 taxi/run (rails + IMU spectra).

---

## 11. References

- BOM: `/workspace/bom/BOM-v0.1.md`  
- Plan §3: `/workspace/actprove-drone/V1_PLAN.md`  
- Nose ICD: `/workspace/airframe-iface/NOSE_ICD_V1.md`  
- Open blockers: `/workspace/actprove-drone/NOTES.md`  

**End of audit.**
