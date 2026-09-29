# Hardware stack — N250 jet airframe (V1 EO-only)

**PM 2026-09-16.** Given by user: **airframe + jet motor N250 only**. Everything else is our design/buy.  
**No thermal / night vision in V1** (FLIR ADK → V1.1). Primary see path = Basler GS GMSL2 nose EO.

## What we already have
| Item | Status |
|------|--------|
| Airframe (ACT-class / user jet airframe) | GIVEN |
| Jet motor **N250** | GIVEN |
| Jet ECU / fuel / plumbing as installed with motor | Assume with motor kit — verify with airframe |

## What we must add (full internals)

### A. Compute island
| # | Item | Qty | Notes | Buy |
|---|------|-----|-------|-----|
| A1 | NVIDIA Jetson **Orin NX 16GB** | 1 | Perception + tracking + SM + logging | BUY |
| A2 | Carrier **Forecr DSBOARD-ORNX + DSADDON-GMSL-NX** | 1 | Lead; **HOLD PO** until Integration PASS (HTE/PPS/FSYNC/mass) | HOLD |
| A3 | Sync mezzanine (custom NRE) | 1 | PPS → FSYNC EO + ADIS DRDY→HTE | BUY/NRE |
| A4 | Active Orin cooling | 1 | Jet bay heat | BUY |
| A5 | NVMe SSD (M.2) | 1 | Black-box ≥1–2 h | BUY |

### B. Flight control & C2
| # | Item | Qty | Notes | Buy |
|---|------|-----|-------|-----|
| B1 | PX4 FC (Pixhawk 6X / Cube Orange+ class) | 1 | TELEM1=C2, TELEM2=Orin | BUY |
| B2 | Orin↔FC UART harness | 1 | MAVLink | BUY |
| B3 | RFD900x pair (900 MHz) | 1 | C2; **wingtip antenna only** | BUY |
| B4 | Near-field digital video Tx/Rx (1.2 or 2.4) | 1 set | GCS EO view; **not** on RFD900 | BUY (SKU TBD) |
| B5 | ELRS/CRSF RC | 0–1 | Safety HOLD | HOLD |
| B6 | Flight-control servos | 2 or 4 + 1 spare | Count depends on elevon/conventional/V-tail layout; exact PN after hinge-moment calculation | BLOCKED |
| B7 | Redundant servo power/BEC + independent packs | 1 system | Cube does not power servo rail; size to aggregate stall current | BLOCKED |
| B8 | Horns, pushrods, ball links/clevises, hinges, mounts | 1 airframe set | Match airframe geometry; no generic rear-surface kit | VERIFY/BUY |

### C. Navigation sensors (shared PPS domain)
| # | Item | Qty | Notes | Buy |
|---|------|-----|-------|-----|
| C1 | ADIS16470-class IMU | 1 | SPI + DRDY HW-ts; rigid with EO; **isolators ≠ turbine mount** | BUY |
| C2 | ZED-F9P-class GNSS + antenna | 1 | PPS master | BUY |
| C3 | Barometer | 1 | Or FC+HW ts | BUY |
| C4 | Pitot / airspeed | 0–1 | Jet: evaluate; HOLD until airframe ICD | HOLD |

### D. Vision (EO only)
| # | Item | Qty | Notes | Buy |
|---|------|-----|-------|-----|
| D1 | Basler ace 2 GS GMSL2 (BOM: a2A1920-168mgm; alt a2A2048-114mgc pending Tracking stamp) | 1 | Nose; HW trigger/FSYNC | BUY candidate |
| D2 | Telephoto C-mount lens (~5–15° FoV TBD) | 1 | HOLD FoV stamp | HOLD FoV |
| D3 | Nose FAKRA bulkhead + Mini-FAKRA jump | 1 | Airframe FIRM ACCEPT | BUY |
| D4 | GMSL2 FAKRA harness nose→Orin | 1 | CSI nose REJECT | BUY |
| D5 | EO↔IMU rigid bracket + isolators | 1 | Hardpoints, not N250 case | BUY |
| D6 | Ground EO USB cam (range test) | 1 | Bench/GCS; cam_id=2 | BUY cheap USB GS if needed |
| — | FLIR / thermal / night vision | 0 | **OUT of V1** | DEFER V1.1 |

### E. Power (critical on jet)
| # | Item | Qty | Notes | Buy |
|---|------|-----|-------|-----|
| E1 | Flight pack / battery (SKU) | 1 | **USER must name pack V** | BLOCKED |
| E2 | Island DC-DC + input filter | 1 | Pack → Orin carrier; size 60–80 W pk + margin | HOLD PN |
| E3 | Isolated **12 V EO** rail (galvanic ≠ jet ECU) | 1 | Basler ≥5 W budget | HOLD PN |
| E4 | Isolated 5 V / 3.3 V logic rails | 1 | ≠ jet BEC/ECU | HOLD PN |
| E5 | FC / radio BEC as per PX4 ICD | 1 | Do not starve TELEM from Orin rail | BUY with FC |
| E6 | Servo/receiver power branch | 1 redundant system | Separate from Jet ECU and avionics; fused and current-monitored | BLOCKED servo V/A |
| E7 | Jet ECU battery + original engine harness | 1 | Identify JetCat Option A/B; P250-PRO-S-V2 specifies 3S LiPo | VERIFY/BUY |

### F. Integration / mechanical
| # | Item | Notes |
|---|------|-------|
| F1 | Payload bay tray / 3D mounts for Orin+FC | Design |
| F2 | Cable loom plan (power / GMSL / UART / SPI / PPS) | Design |
| F3 | CG & mass budget sheet | Integration + airframe |
| F4 | Vibration isolation for EO+IMU | Mandatory on jet |
| F5 | FTS holder (named person) | **USER** — blocks G2/G3 |
| F6 | Complete control linkage and surface-load calculation | Airframe owner + Integration; mandatory before servo order |
| F7 | JetCat PRO-Interface V2 / GSU and THR/AUX receiver wiring | Original JetCat parts; pilot retains independent engine stop |
| F8 | Connector/loom kit and strain relief | JST-GH, servo contacts, shielded USB3, P-clamps, labels; no Dupont jumpers |

Detailed missing-airframe BOM and staged bench plan:
`AIRFRAME_CONTROLS_TURBINE_BOM_2026-09-18.md`.

## Explicit non-buys (V1)
Radar · FPGA · second SBC · thermal/NV · Starlink/phone LTE · raw 5.8 as only C2 · cameras into FC · USB as primary Orin↔FC · seekers/warheads · Hadron as primary EO.

## Blockers before carrier PO
1. Forecr FAE: HTE GPIO + PPS/FSYNC pin names + addon/cooler mass → email `support@forecr.io`
2. Pack voltage / battery SKU → unlocks PSU buys
3. Named FTS holder → unlocks G2/G3
4. Lens FoV stamp (Tracking + airframe nose window)

## Power sketch (island)
```
[Pack]──filter──► DC-DC ──► Orin carrier (9–24 V in)
              ├──► isol 12 V ──► Basler EO
              ├──► isol 5/3.3 ──► GNSS/IMU logic (as needed)
[Jet ECU / pump] ══ separate ══ (never share camera rail)
[FC BEC] ──► FC + RFD900 preferred dedicated
[Servo packs A/B] ──► redundant servo power ──► Cube servo rail + servos
```
