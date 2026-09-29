# HARDWARE_AUDIT.md — N250 jet + EO-only V1

**Owner:** hardware  
**Stamp:** 2026-09-16 CT  
**Scope:** Airframe + N250 jet are **GIVEN**. This audit lists **everything else** to design / buy / NRE for V1 G1 (EO-only). No thermal.  
**Authority peers:** `BOM-v0.1.md`, `BUY_LIST.md`, `V1_PLAN.md`, `NOTES.md`  
**Status vs carrier:** Forecr path = **Directional GO / HOLD PASS** — do **not** PO carrier+addon until Integration PASS.

---

## 0. Given (out of this audit’s buy list)

| Given | Notes |
|-------|--------|
| N250 jet airframe | Structure, turbine, fuel, basic harness spaces — owned by airframe |
| Nose volume / hardpoints | Isolator mounts exist; EO+IMU **not** turbine-bolted |
| Pack / bus (concept) | Voltage map still **UNKNOWN** → PSU PNs HOLD |
| Flight surfaces / actuators | FC-side; not reinvented here |

---

## 1. Executive verdict

| Topic | Call |
|-------|------|
| V1 sensors | **EO-only** Basler GS GMSL2. Thermal/ADK/Horus/Boson **DEFER V1.1** — do not PO. |
| Compute island lead | **Forecr DSBOARD-ORNX (85 g) + DSADDON-GMSL-NX** — SPI0 @ 3.3 V **PASS**; Mini-FAKRA default |
| Carrier PO | **BLOCKED** — DRDY→HTE **FAIL**; PPS/FSYNC unnamed; addon+cooler mass UNKNOWN |
| Sync | Custom **sync mezz BUY/NRE** required; soft SPI / soft DRDY **REJECT** |
| Island mass (known) | SOM 28 g + carrier 85 g = **113 g** + addon(?) + cooler(?) + mezz ~8–15 g ≪ 0.5–0.8 kg gate on knowns |
| Airframe interconnect | **FAKRA bulkhead FIRM ACCEPT**; bare MMCX into nose **REJECT**; Mini-FAKRA→FAKRA jump **BUY** |
| Demoted / reject carriers | NGX018 (no SPI), Miivii APEX (mass), Leopard (no SPI), CSI-only, Seeed 404 |

**Thoughts vs Forecr HOLD (updated):**  
SPI0 close was the right pivot off NGX018. Forecr remains the best **published** GMSL+SPI0 path, but Nav Must-PASS still fails without a **named HTE/AON 3.3 V GPIO** for ADIS DRDY. Until Forecr FAE answers (draft ready → `support@forecr.io`, awaiting send), treat Forecr as **design baseline**, not a buy. Parallel: keep sync-mezz NRE scoped so pin names drop in when FAE replies. Do **not** reopen NGX018 scavenger or soft SPI.

---

## 2. Topology (V1 EO-only)

```
                    [GIVEN: N250 airframe + jet]
                              │
   Nose EO Basler GS GMSL2 ──FAKRA bulkhead──► Mini-FAKRA jump ──► DSADDON-GMSL-NX
                                                                    │ CSI ribbons
   GNSS F9P PPS ──► sync mezz ──► Orin (time) + FSYNC 30/60 ──► EO only (V1)
   ADIS16470 ──SPI0 DF11──► Orin   DRDY ──► HTE GPIO (FAE TBD)
                              │
                    DSBOARD-ORNX + Orin NX 16GB + NVMe + cooling
                              │ UART MAVLink TELEM2
                              ▼
                    PX4 FC ──TELEM1──► RFD900x (ant = wingtip only)
                              │
                    Near-field video Tx (separate RF) ──► ground Rx
```

**Hard rules:** cams → Orin only (never FC); USB not primary Orin↔FC; EO+IMU on isolators to hardpoints (**not** turbine); galvanic-isolated cam rails ≠ jet ECU/ESC noise; no radar/FPGA/2nd SBC V1.

---

## 3. Everything else to design / buy (complete checklist)

Status legend: **BUY** = approved class this week · **BUY candidate** = preferred PN pending gate · **BUY/NRE** · **HOLD** · **DEFER V1.1** · **REJECT** · **GIVEN**.

### 3.1 Compute island

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| C1 | NVIDIA Jetson Orin NX 16GB module | Buy module | **BUY** | Final weigh still |
| C2 | Forecr DSBOARD-ORNX | Carrier | **BUY candidate BLOCKED** | Integration PASS |
| C3 | Forecr DSADDON-GMSL-NX (Mini-FAKRA) | GMSL deserial; V1 uses **1× EO** lane | **BUY candidate BLOCKED** | Addon mass + PASS |
| C4 | Orin cooler / TTP (active preferred) | Thermal path for island | **BUY** class / mass HOLD | Forecr cooler mass UNKNOWN |
| C5 | NVMe SSD (M.2) ≥1–2 h black-box | Logging | **BUY** | — |
| C6 | Sync mezzanine (custom) | PPS buffer, FSYNC 30/60, DRDY→HTE latch | **BUY/NRE** | FAE pin names |
| C7 | Island DC-DC + EMI filter | Pack → carrier; size 60–80 W peak + margin | **HOLD** PN | Pack V unknown |
| C8 | Vibration isolators / island tray | Mount island to airframe hardpoints | **BUY** class | CG with airframe |

### 3.2 EO (V1 only — no thermal)

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| E1 | Basler ace 2 a2A1920-168mgm GS GMSL2 | Primary EO | **BUY candidate** | Carrier PASS for full chain |
| E2 | Telephoto C-mount lens ~5–15° FoV | Optics for long-range tiny UAV | **HOLD** FoV | Range × target-size sheet |
| E3 | Nose optical window (≥40 mm class) | Flush GS window | Airframe + hw | Mold depth TBD |
| E4 | GMSL2 / FAKRA nose harness | Nose → bulkhead → island | **BUY** | — |
| E5 | Mini-FAKRA → FAKRA bulkhead jump | Island-side adapter harness | **BUY** (FIRM ACCEPT) | — |
| E6 | EO vibration isolation bracket | Rigid with IMU; isolators → hardpoints | **BUY** | Lever-arm survey |
| E7 | FLIR ADK / Horus / Boson / TL640 | Thermal | **DEFER V1.1** | Do not PO |

### 3.3 Navigation sensors

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| N1 | ADIS16470-class IMU | Free-run + DRDY HW-ts; SPI0 | **BUY** | DRDY→HTE path |
| N2 | IMU rigid mount + lever arm to EO | Extrinsics fixture | **BUY** | Survey after mount |
| N3 | ZED-F9P class + antenna | Multi-band GNSS; PPS master | **BUY candidate** | Antenna placement |
| N4 | Barometer | Air data / vertical aid | **BUY** | — |
| N5 | Pitot / airspeed | Preferred on jet | **HOLD** | Airframe plumbing |
| N6 | VN-100 as SPI workaround | — | **REJECT** as ADIS swap | Keep ADIS |

### 3.4 Flight controller & links

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| F1 | PX4 FC (Pixhawk 6X / Cube Orange+ class) | TELEM1=C2, TELEM2=Orin | **BUY** | — |
| F2 | Orin↔FC UART harness | TELEM2 MAVLink @ 57600 | **BUY** | — |
| F3 | RFD900x-class modem pair | C2 on TELEM1 | **BUY** | — |
| F4 | RFD antenna + wingtip mount | Ant **wingtip only** | **BUY** | — |
| F5 | Near-field digital video Tx 1.2/2.4 | Separate RF from Orin/cam | **BUY** V1 | Exact SKU TBD |
| F6 | Near-field digital video Rx | Rail/tower | **BUY** V1 | Exact SKU TBD |
| F7 | ELRS/CRSF third link | RC port | **HOLD** | Safety disposition |

### 3.5 Power (isolated from jet ECU)

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| P1 | Pack Vnom/Vmin/Vmax or battery SKU | Size all DC-DC | **HOLD** | Platform data |
| P2 | Galvanic-isolated **12 V** EO rail | ≥5 W class headroom; ≠ ESC/ECU | **HOLD** PN (req PASS) | Pack V |
| P3 | Galvanic-isolated **5 V / 3.3 V** logic rails | IMU/GNSS/mezz; ≠ BEC | **HOLD** PN (req PASS) | Pack V |
| P4 | Star ground / single-point return plan | With airframe-iface | **DESIGN** | — |
| P5 | Current sense / log (optional) | Island health | **HOLD** | — |

**Rail budget (planning):** EO Basler ≥5 W @ 12 V; nav IMU+GNSS+misc <3 W on 3.3/5; Orin island 15–30 W sustained / 60–80 W peak.

### 3.6 Mechanical / environmental

| # | Item | Design / buy action | Status | Blocker |
|---|------|---------------------|--------|---------|
| M1 | EO+IMU isolated nose bracket | **Not** turbine-hard-bolted | **BUY** / DESIGN | — |
| M2 | Cable strain relief + EMI shields | GMSL + power | **BUY** / DESIGN | Cable lengths TBD |
| M3 | Thermal path for Orin (duct/heatsink) | Active cooling integration | **DESIGN** | Cooler mass |
| M4 | CG / mass worksheet | Island dry + harness | **HOLD** | Addon+cooler+pack |

### 3.7 Explicit non-buys (V1)

Radar · FPGA · second SBC · seekers/warheads · cameras into FC · USB as primary Orin↔FC · soft SPI/soft DRDY · CSI at nose · bare MMCX into nose bay · RFD ant in nose/exhaust · EO/IMU on turbine mount · Starlink/phone LTE as C2 · HD video on RFD900 · thermal cameras (V1.1) · Miivii APEX · NGX018 as primary · Leopard for SPI · VN-100 to “fix” carrier.

---

## 4. Forecr HOLD — detailed gate board

| Gate | Status | Evidence / next |
|------|--------|-----------------|
| SPI0 DF11-16 @ 3.3 V free w/ GMSL addon | **PASS** | Addon on CSI CAM0/CAM2; level-shifted 3.3 V OK for ADIS; CS# polarity FAE |
| Mini-FAKRA → FAKRA bulkhead | **BUY / FIRM ACCEPT** | Airframe |
| DRDY → named HTE/AON GPIO @ 3.3 V | **FAIL** | No PEE.02-class pin in Forecr docs; FAE ask #1 |
| PPS in + cam FSYNC named | **HOLD** | FAE asks #2–3; V1 = FSYNC for Basler only |
| Addon + cooler mass | **HOLD** | FAE ask #4; known island 113 g OK vs 0.5–0.8 kg |
| Nav re-PASS | **HOLD PASS** | Needs one pin table: SPI0 + HTE DRDY + PPS + FSYNC |
| Integration PASS / carrier PO | **NO** | Until above close |

**FAE (5 asks) — draft ready, not yet sent:**  
`support@forecr.io` — HTE GPIO, PPS path, FSYNC outs, DSADDON+cooler mass, SPI0 CS# polarity.  
(Gmail connector absent; awaiting Zahar send or contact-form submit.)

---

## 5. Design work (not a catalog line)

| Workstream | Deliverable | Owner |
|------------|-------------|--------|
| Sync mezz ICD | Block diagram + Forecr pin table (post-FAE) | hardware + Nav |
| Extrinsics | EO↔IMU lever arm survey procedure | hardware + Nav |
| Time sync bring-up | PPS → Orin → FSYNC → frame ts; latency probe | System stack + hw |
| Power ICD | Pack map → isolated rails schematic | hardware + airframe-iface |
| Nose ICD | Window, FAKRA pass-through, isolator mounts | airframe-iface + hw |
| Black-box layout | NVMe partitions, rates, ≥1–2 h | System stack + hw |
| Mass/CG sheet | Weigh SOM, carrier, addon, cooler, mezz, harness | hardware + airframe |

---

## 6. Recommended buy sequence (discipline)

1. **This week (no carrier):** Orin NX module, sync-mezz NRE kickoff, FAKRA jump, UART harness, RFD pair, NVMe, cooling class, EO+IMU mount kit, Basler candidate, ADIS, F9P+ant, baro, PX4 FC — per `BUY_LIST.md`.  
2. **Send Forecr FAE** → lock HTE/PPS/FSYNC + addon mass.  
3. **Nav + Integration re-PASS** on one drawing.  
4. **Then** PO Forecr carrier + GMSL addon + cooler.  
5. **V1.1:** thermal ADK path on second GMSL lane.

---

## 7. Open data still required from platform

| Datum | Blocks |
|-------|--------|
| Pack Vnom/Vmin/Vmax (or SKU) | DC-DC / isolator PNs |
| Cable lengths nose→island | Harness length / EMI |
| Addon + cooler grams (or weigh) | Final mass stamp |
| Detection range × target size | Lens FoV / focal length |
| Forecr FAE written answers | Integration PASS |

---

## 8. Hardware stack contribution (pointers)

| Artifact | Role |
|----------|------|
| This file | Full “given airframe → buy/design everything else” audit |
| `BOM-v0.1.md` | Stamped buy-from table |
| `BUY_LIST.md` | This-week buys vs HOLD |
| `V1_PLAN.md` §2–§3 | Topology + BOM freeze |
| `NOTES.md` | Critical path B1/B12 FAE, B3 pack V |
| `docs/SYSTEM_STACK.md` | Software ICD / bring-up ownership (not physical BOM) |

**Hardware owns:** physical island, sensors (EO/IMU/GNSS class), sync mezz NRE, harness/power class, mounts, Forecr FAE chase, mass worksheet.  
**Does not own:** mission SM policy, track filter, guidance, FTS policy, GCS UI.

---

## 9. Change log

| When | Change |
|------|--------|
| 2026-09-16 | Initial audit: N250 given; EO-only V1; Forecr HOLD detailed; full buy/design checklist |
