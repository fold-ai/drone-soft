# Week-delivery cart (≈7 days) — Amazon-first + fast EO alt (PM 2026-09-18)

**Constraint:** arrive ~1 week. Prefer Amazon/Prime. Basler dart may miss → **Arducam IMX296 USB3** interim.

**Have:** airframe + GoPro (DEMO only).

## BUY THIS WEEK (verify ZIP for Prime date before pay)

| # | Item | Where | Notes |
|---|------|-------|-------|
| 1 | **Cube Orange+ Standard Set** | Amazon [B0C8Y1LMGZ](https://www.amazon.com/dp/B0C8Y1LMGZ) or GetFPV in-stock | Prefer **set** (cube+carrier+PM). Alt GetFPV same-day ship. |
| 2 | **Here3+** | Amazon / IR-LOCK / GetFPV | Dedicated Cube GPS |
| 3 | **TX16S MKII ELRS** | Amazon e.g. [B0DXTVKJD6](https://www.amazon.com/dp/B0DXTVKJD6) / [B0CSSR1YJV](https://www.amazon.com/dp/B0CSSR1YJV) | Mode 2 ELRS internal |
| 4 | **ELRS RX** (RP1/RP2/HappyModel EP) | Amazon | Air RX for abort+CH7/CH8 |
| 5 | **RFD900x-US FCC bundle** | **IR-LOCK** (not Amazon) [bundle](https://irlock.com/products/rfd900x-us-modem-bundle-fcc-approved) ~$279 in stock | US ground ship usually days; do **not** buy non-US clone |
| 6 | **Orin path (fast)** | Amazon **Seeed reComputer J4012** Orin NX 16GB [B0C88V4CB7](https://www.amazon.com/dp/B0C88V4CB7) | Heavy for flight island — OK for **bench + first air** if mass allows after strip enclosure; or Seeed bare module+carrier if ships ≤7d |
| 7 | **FAST EO (stamp this week)** | Amazon **Arducam IMX296 GS USB3** [B0DBV4CBDQ](https://www.amazon.com/dp/B0DBV4CBDQ) ~$170–181 | Global shutter, UVC, ~44 fps — **week interim**. Soft ts OK. |
| 8 | UART TELEM2 | Digi-Key GHR-06V-S + contacts **or** Holybro 1186 | Often 1–3d Digi-Key |
| 9 | NVMe 128–512GB (if not in J4012) | Amazon | |
| 10 | Expendable **test drone** | Amazon / local | + photos |

## Camera decision
| Choice | When |
|--------|------|
| **Arducam IMX296 USB3 (Amazon)** | **Order now** for ≤7d |
| Basler daA1920-160uc + Edmund | Order parallel if ok with longer lead — upgrade path |
| GoPro | DEMO Mac only — not Lock |

## DO NOT BUY for week gate
Forecr · FIM · Basler ace+40mm · sync mezz · non-FCC 900 MHz

## Risk
- J4012 enclosure mass — strip to carrier+SOM if over 680 g companion
- Arducam 44 fps < 60 target — acceptable first gate; manual exp if AE sticks
- Confirm all Amazon **Prime ≤7 days** to your ZIP before checkout

## UPDATE 2026-09-18 evening — camera + RC/C2 clarified

### Camera (user rejected IMX296 44 fps)
| Rank | Cam | FPS | Where | Note |
|------|-----|-----|-------|------|
| **A week Amazon** | Arducam **OV9281 mono** GS USB ~100–120 fps MJPG | [B096M5DKY6](https://www.amazon.com/dp/B096M5DKY6) / [B0FXWWF55X](https://www.amazon.com/dp/B0FXWWF55X) | Mono OK for detect; set MJPG high fps |
| **B better (order e-con)** | **See3CAM_24CUG** AR0234 color GS | HD **120** / FHD **60** | e-con Systems — check ship ≤7d |
| **C upgrade** | Blackfly S IMX287 / Basler dart | high fps | longer lead |
| **REJECT week Lock** | Arducam IMX296 B0499/B0641 | **44 fps** | too slow for fast flight |

### Links (do not mix roles)
1. **ELRS** = pilot sticks + CH7/CH8 + **abort** (must not drop) — handset ↔ air RX ↔ Cube
2. **RFD900x-US** = GCS telem / Kill / ABORT mirror (separate modem) — laptop ↔ air modem ↔ Cube TELEM1
Redundancy: lose RFD → still fly/abort on ELRS; lose ELRS → failsafe (Safety).

### Exact TX to buy
**RadioMaster TX16S MKII ELRS Mode 2** Amazon: https://www.amazon.com/dp/B0DXTVKJD6  
+ **RP1** air RX: https://www.amazon.com/dp/B0BY1B859X  
Map CH7=Engage SEARCH, CH8=TEST, spare for abort switch per Safety.
