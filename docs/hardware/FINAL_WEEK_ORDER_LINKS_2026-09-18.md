# Final week order stack — links (PM 2026-09-18)

**Mission:** climb → TEST → SEARCH → red box → Kill → CLOSE @ ~150 m on ~0.5×0.75 m test drone.  
**Mass A:** companion island ≤680 g (weigh). **Cloud ≠ C2.**

Verify Prime/ship date to your ZIP before pay.

---

## A. Code computer (Orin — all detect/track/Kill code)

| # | Item | Link |
|---|------|------|
| 1 | **Seeed reComputer J4012** Jetson **Orin NX 16GB** | https://www.amazon.com/dp/B0C88V4CB7 |
| 1b | Alt w/ power cable | https://www.amazon.com/dp/B0DSZRGXRF |

Strip enclosure if mass tight. USB3 ports for both cams.

---

## B. Dual nose cameras (week — no wait for Basler)

| # | Role | Item | Link |
|---|------|------|------|
| 2 | **SEARCH wide** | ELP AR0234 USB3 **board ~100°** | https://www.amazon.com/dp/B0H3Q77QDG |
| 3 | **TELE Lock ~150 m** | ELP AR0234 USB3 **5–50 mm zoom** — set **~50 mm** | https://www.amazon.com/dp/B0H4K4KDKT |

Weigh zoom on receipt (overflow risk if ≫200 g).

**Later upgrade (optional now):** Basler daA1920-160uc #108233 + Edmund #70-644 35 mm.

---

## C. Pilot radio (ELRS — sticks / TEST / Engage / abort)

| # | Item | Link |
|---|------|------|
| 4 | **RadioMaster TX16S MKII ELRS Mode 2** | https://www.amazon.com/dp/B0DXTVKJD6 |
| 5 | **RadioMaster RP1** ELRS RX (air) | https://www.amazon.com/dp/B0BY1B859X |

Map: CH8=TEST, CH7=SEARCH, abort switch. Antennas: use kit / RP1 U.FL whip.

---

## D. GCS data link (RFD — telem / Kill / ABORT mirror)

| # | Item | Link |
|---|------|------|
| 6 | **RFD900x-US FCC modem bundle** (2× air+gnd, ants, FTDI, Pixhawk cable) | https://irlock.com/products/rfd900x-us-modem-bundle-fcc-approved |

Do **not** buy non-US 900 MHz clones. Antennas included in bundle.

---

## E. Autopilot

| # | Item | Link |
|---|------|------|
| 7 | **Cube Orange+ Standard Set** (cube+carrier+PM) | https://www.amazon.com/dp/B0C8Y1LMGZ |
| 8 | **Here3+ GPS** | https://www.amazon.com/dp/B0C9NGH16Z |
| 9 | TELEM2 UART harness | Digi-Key **GHR-06V-S** https://www.digikey.com/en/products/detail/jst-sales-america-inc/GHR-06V-S/807185 + contacts SSHL-002T-P0.2 **or** Holybro 1186 if in stock |

TELEM1↔RFD cable often **in RFD bundle**.

---

## F. Target + power

| # | Item | Link |
|---|------|------|
| 10 | Expendable test drone ~50×75 cm | Amazon search “FPV drone bind and fly” / local — pick known size + take photos |
| 11 | Batteries / charger for interceptor + target | Amazon / hobby shop |

---

## G. Airframe controls + turbine interface (missing from original list)

| # | Item | Qty / status |
|---|---|---|
| 12 | Digital HV metal-gear servos | 2 for elevons or 4 for conventional/V-tail + 1 spare; **BLOCKED on torque calculation** |
| 13 | Servo frames, horns, M3/M4 pushrods, ball links/clevises, hinges, safety retainers | One airframe set; VERIFY/BUY after measurements |
| 14 | External redundant servo-power controller/BEC + independent packs | 1 system; **BLOCKED on servo voltage and total stall current** |
| 15 | Servo wiring/distribution/fuses/current monitoring | One loom; size after load calculation |
| 16 | JST-GH/servo connector kit, shielded USB3 leads, crimpers, strain relief, labels | One bench/aircraft kit |
| 17 | Digital airspeed sensor + pitot/tube/mount | 1; recommended before automatic speed/landing modes |
| 18 | Original JetCat harness for exact engine Option A/B | VERIFY |
| 19 | JetCat PRO-Interface V2 + GSU/programmer | VERIFY/BUY if not included |
| 20 | Dedicated JetCat ECU 3S LiPo | VERIFY capacity against exact engine manual |
| 21 | Fuel-rated tank/plumbing/fill/vent/filter + heat shielding + manual shutoff | **BLOCKED on airframe/endurance** |

The Cube does not power the servo rail. Use separately fused branches for
turbine ECU, servo/receiver power, and avionics/Jetson. Pilot throttle and
engine stop remain independent of the vision computer for the first flight.

Full Ukrainian BOM, connector map and test sequence:
[`AIRFRAME_CONTROLS_TURBINE_BOM_2026-09-18.md`](AIRFRAME_CONTROLS_TURBINE_BOM_2026-09-18.md).

---

## H. Explicit DO NOT BUY this gate

Forecr · FIM · 3× zoom · Basler ace+40 mm required-now · non-FCC RFD clones · thermal

---

## Order checklist

- [ ] Amazon: Orin J4012, Cube set, Here3+, TX16S, RP1, ELP board, ELP zoom  
- [ ] IR-LOCK: RFD900x-US bundle  
- [ ] Digi-Key: UART (if needed)  
- [ ] Measure control surfaces/servo bays and identify airframe layout  
- [ ] Identify JetCat Option A/B from nameplate and connector photos  
- [ ] Freeze servo torque/current/power/wire/fuse calculation before ordering actuators  
- [ ] Buy linkage, loom and original JetCat interface parts after verification  
- [ ] Test drone + packs  
- [ ] Weigh companion island before flight  
