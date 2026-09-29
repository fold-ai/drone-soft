# Buy this week (US) — 2-week RF gate (PM 2026-09-18)

Stock checked via public web search 2026-09-18. **Verify cart before pay** — inventory moves.

**Best Buy:** effectively **empty** for this stack (no Cube / RFD / Orin / Basler). Skip.

## Tier A — order now (unblocks 2-week HEARTBEAT + cue/WORK)

| # | Item | Where (US) | Notes / ~price |
|---|------|------------|----------------|
| 1 | **RFD900x-US modem bundle** (2× FCC + antennas + **FTDI USB**) | [IR-LOCK](https://irlock.com/products/rfd900x-us-modem-bundle-fcc-approved) ~$279 in stock; also OneDrone / Unmanned Tech / SpektreWorks | **Prefer FCC-US bundle**. Amazon has single modem listings — prefer full bundle. |
| 2 | **Cube Orange+** (+ carrier if you don't have one) | [IR-LOCK](https://irlock.com/products/the-cube-orange-plus) ~$277 in stock; SpektreWorks; Amazon standalone cube | Need **carrier board** + power module for TELEM ports. RMRC full kit may be delayed (check date). |
| 3 | **Here3+ GPS** (dedicated Cube GPS) | IR-LOCK / RMRC / Amazon HX4-06213 | Do **not** share Orin F9P with Cube. |
| 4 | **ELRS radio path** | Amazon: **TX16S MKII ELRS** (~$260) **or** existing radio + Ranger Micro/Ranger module + RP1/RP2 RX | Safety: override only ≠ RFD C2. |
| 5 | **Cube TELEM2↔Orin UART** | Holybro SKU **1186** — holybro.com often Sold Out; alt TechRoLK / SPEXDRONE; **or Digi-Key DIY** GHR-06V-S (455-1596-ND, large stock) + SSHL-002T-P0.2 | **Leave 5V unconnected.** |
| 6 | **Cube TELEM1↔RFD cable** | Often **in RFD bundle** (Pixhawk cable); else JST-GH 6→6 20–30 cm | Prefer power-removed. |

## Tier B — order this/next week (parallel, not 2-week RF blocker)

| Item | Where | Notes |
|------|-------|-------|
| Jetson **Orin NX 16GB** module | Digi-Key / Arrow / Seeed / ThinkRobotics (check live qty) | Needs **carrier** later (Forecr FAE HOLD) — bench power OK |
| **Basler a2A1920-168mgm** + **40 mm C-mount** | Basler / authorized US distributors | Long lead; buy for later install — not RF gate |
| **IFLY FIM-2410** pair | IFLY / UAV video dealers | Optional SA video; ≠ C2 |
| NVMe 1–2 TB M.2 | Amazon / Best Buy / Newegg | Logging — Best Buy OK for consumer SSD |
| USB-C PD / bench PSU 5–20 V | Amazon / Best Buy | Bench only |
| Breadboard / 3D-print filament / clamps | Amazon / Best Buy / local | Interim EO mount |

## Tier C — do NOT buy this week

| Item | Why |
|------|-----|
| Forecr DSBOARD-ORNX | FAE / Integration PASS |
| sync mezz NRE PCB | DEFER past 2-week |
| Flight CNC EO+IMU mount | DEFER; 3D-print interim |
| FTS live RF | Holder unnamed |
| Thermal FLIR | V1.1 |
| Non-US / non-FCC 900 MHz clones | Compliance |

## Suggested cart sequence (this week)
1. IR-LOCK: RFD900x-US bundle + Cube Orange+ (+ carrier kit if needed) + Here3+  
2. Amazon: TX16S MKII ELRS **or** Ranger module + RX (+ batteries if needed)  
3. Digi-Key: GHR-06V-S + contacts (if Holybro 1186 OOS)  
4. Amazon/Best Buy: NVMe + bench PSU only  

## Explicit
Mac GoPro DEMO stays software. 2-week success = RFD HEARTBEAT + cue/WORK — not Lock PASS.
