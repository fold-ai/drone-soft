# Final week buy list (English) — Manual Engage first test

**Goal:** manual climb → TEST → SEARCH → red box → Kill → CLOSE (~150 m) on ~0.5×0.75 m test drone.  
**Companion island mass:** ≤1.5 lb (~680 g) — weigh before flight.  
**Cloud (Vercel/Supabase):** logs/training only — never flight C2.

Verify stock and delivery date for your ZIP before paying.

---

## 1. Code computer (runs detect / track / Kill)

| # | Item | Buy |
|---|------|-----|
| 1 | **NVIDIA Jetson Orin Nano Super Developer Kit** (8GB) — recommended (cheaper than NX 16GB) | https://www.amazon.com/dp/B0BZJTQ5YP |
| 1b | Alt: Seeed reComputer **J3011** Orin Nano 8GB | https://www.amazon.com/dp/B0F1N37BL9 |
| 1c | Alt (more headroom): Seeed **J4011** Orin NX 8GB | https://www.amazon.com/dp/B0DZGM7SKR |

Do **not** need J4012 Orin NX 16GB for this first test.

---

## 2. Dual nose cameras

| # | Role | Item | Buy |
|---|------|------|-----|
| 2 | **SEARCH (wide)** | ELP AR0234 USB3 board ~100° FOV | https://www.amazon.com/dp/B0H3Q77QDG |
| 3 | **TELE Lock (~150 m)** | ELP AR0234 USB3 **5–50 mm zoom** — set lens to **~50 mm** | https://www.amazon.com/dp/B0H4K4KDKT |

Weigh the zoom camera on receipt (mass risk vs 680 g island).  
Optional later upgrade: Basler daA1920-160uc #108233 + Edmund #70-644 35 mm.

---

## 3. Pilot radio (ELRS — sticks, TEST, Engage, abort)

| # | Item | Buy |
|---|------|-----|
| 4 | **RadioMaster TX16S MKII ELRS Mode 2** | https://www.amazon.com/dp/B0DXTVKJD6 |
| 5 | **RadioMaster RP1** ELRS receiver (aircraft) + kit antenna | https://www.amazon.com/dp/B0BY1B859X |

Channel map: CH8 = TEST, CH7 = SEARCH, dedicated abort switch.

---

## 4. GCS data link (RFD — telemetry / Kill / ABORT)

| # | Item | Buy |
|---|------|-----|
| 6 | **RFD900x-US FCC modem bundle** (2 modems + antennas + FTDI USB + Pixhawk cable) | https://irlock.com/products/rfd900x-us-modem-bundle-fcc-approved |

Do **not** buy non-US / non-FCC 900 MHz clones. Antennas are included.

---

## 5. Autopilot (Amazon Standard Set often OOS — buy here)

| # | Item | Buy |
|---|------|-----|
| 7 | **Cube Orange+ Standard Set** (cube + carrier + power module) | https://irlock.com/products/cube-orange-plus-standard-set |
| 7b | Alt (same-day ship often): GetFPV Cube Orange+ Standard Set | https://www.getfpv.com/electronics/flight-controllers/airplane-fc/cubepilot-the-cube-orange-standard-set.html |
| 8 | **Here3+ GPS** | https://www.amazon.com/dp/B0C9NGH16Z |
| 9 | TELEM2 UART (Cube↔Orin) | Digi-Key GHR-06V-S https://www.digikey.com/en/products/detail/jst-sales-america-inc/GHR-06V-S/807185 (+ SSHL-002T-P0.2 contacts) |

TELEM1↔RFD cable is usually in the RFD bundle.

---

## 6. Target + power

| # | Item | Buy |
|---|------|-----|
| 10 | Expendable **test drone** ~50×75 cm + photos for training | Amazon / local hobby shop |
| 11 | Batteries / charger for interceptor + target | Amazon / hobby shop |

---

## 7. Airframe controls, connectors, and turbine interface — previously missing

Do **not** order an arbitrary high-torque servo yet. Exact servo SKU, servo voltage,
power controller, wire gauge, and fuse ratings are blocked on airframe geometry and
the first-flight speed envelope.

| # | Item | Qty | Status |
|---|---|---:|---|
| 12 | Matched digital HV metal-gear servos | 2 (delta/elevon) or 4 (conventional/V-tail) + 1 spare | **BLOCKED: torque calculation** |
| 13 | Servo frames, control horns, M3/M4 pushrods, ball links/clevises, hinges, safety clips | One complete set | VERIFY/BUY after measuring airframe |
| 14 | External redundant servo-power controller / dual-input BEC | 1 | **BLOCKED: servo voltage and total stall current** |
| 15 | Independent servo/receiver batteries | 2 | **BLOCKED: item 14** |
| 16 | Servo distribution/fuse/current monitoring + twisted 20–22 AWG extensions | 1 loom | BUY after load calculation |
| 17 | Digital airspeed sensor + pitot/tubing/mount | 1 | RECOMMENDED before automatic speed/landing modes |
| 18 | JST-GH TELEM/CAN pigtails and spare contacts; servo connectors; proper crimp tools | 1 kit | BUY |
| 19 | Short shielded USB3 camera cables with clamps/strain relief | 2 | BUY after confirming camera-side connector |
| 20 | Braided sleeve, heat-shrink, grommets, P-clamps, ferrites, labels | 1 loom kit | BUY |
| 21 | JetCat original engine/data/power harness matching **Option A or B** | 1 | VERIFY exact engine version |
| 22 | JetCat PRO-Interface V2 + GSU/programmer | 1 each | VERIFY/BUY if not in engine set |
| 23 | Dedicated JetCat ECU battery | 1 | VERIFY current engine manual; P250-PRO-S-V2 lists 3S LiPo |
| 24 | Fuel-rated tank/plumbing/filter/fill/vent hardware, heat shielding, manual shutoff | 1 system | **BLOCKED: airframe and endurance** |
| 25 | Multimeter, current/power analyzer, servo tester, pin extractors | 1 bench kit | BUY if missing |

The Cube does **not** power the servo rail. Keep turbine ECU, servo/receiver,
and avionics/Jetson power as three separately fused branches. For the first gate,
the pilot must retain independent throttle and engine-stop authority; the vision
computer must not control turbine start/stop/throttle.

Full selection rules, connector map, JetCat interface and staged test plan:
[`AIRFRAME_CONTROLS_TURBINE_BOM_2026-09-18.md`](AIRFRAME_CONTROLS_TURBINE_BOM_2026-09-18.md).

---

## Do NOT buy for this gate

- Forecr carrier  
- FIM-2410 video  
- Three zoom cameras  
- Basler ace + 40 mm as a blocker for this week  
- Non-FCC RFD clones  
- Thermal  

---

## Order checklist

- [ ] Amazon: Orin Nano Super Dev Kit, Here3+, TX16S, RP1, ELP wide board, ELP zoom  
- [ ] IR-LOCK: Cube Orange+ Standard Set + RFD900x-US bundle  
- [ ] Digi-Key: UART if needed  
- [ ] Measure airframe/control surfaces; identify delta, conventional, or V-tail layout  
- [ ] Photograph JetCat nameplate and connector; identify Option A or Option B  
- [ ] Calculate servo torque, total stall current, BEC/power controller, batteries, wire and fuses  
- [ ] Buy mechanics, loom parts, original JetCat interface/harness, and bench tools  
- [ ] Test drone + packs  
- [ ] Weigh companion island before flight  
