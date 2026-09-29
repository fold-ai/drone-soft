# Airframe Audit — N250 Jet + Payload Bay Interfaces

**Doc:** `docs/audit/AIRFRAME_AUDIT.md`  
**Owner:** airframe-iface  
**Stamp:** 2026-09-16 18:50 CT  
**Platform:** existing airframe + **JetCat P250-class (N250) turbojet** only  
**Does not change:** wing span, sweep, fuel mass / tank geometry  

Related: Nose ICD V1 (`/workspace/airframe-iface/NOSE_ICD_V1.md`), Integration BOM stamps, `V1_PLAN.md` B3/B4/B11.

---

## 1. Scope

This audit defines **payload-bay and nose interfaces** for the V1 compute / EO stack on the existing N250 airframe. Airframe-iface owns holes, bulkheads, isolation, antenna placement, and mass/CG placeholders. It does **not** redesign the wing or resize fuel.

| Owns | Does not own |
|------|----------------|
| Nose window + flush fairing | Orin carrier pinmux / FAE |
| FAKRA bulkhead + harness keep-outs | PX4 / SAFETY state machine |
| EO+IMU rigid bracket isolators | Detection / Tracking software |
| Isolated pack→island / cam rails | FTS holder naming |
| Wingtip RFD900 antenna hole | Lens FoV buy (Tracking) |
| CG / mass budget placeholders | Span / sweep / fuel mass |

---

## 2. Propulsion / ECU context (N250)

| Item | Value (datasheet class) | Airframe note |
|------|-------------------------|---------------|
| Engine | JetCat P250-PRO-S class (~250 N, ~2155 g) | Turbine mount = vibration/heat source |
| ECU supply (engine) | **10–30 V DC** (35 V max); ideal **3–4S LiPo/LiFe**, >2000 mAh | Starter peaks ~16 A @ 12 V — **noisy, high di/dt** |
| Running draw (engine) | ~8 W idle / ~34 W max (order of) | Keep avionics off this bus |

**Hard rule:** camera rails and Orin island input are **galvanically isolated** from the jet ECU / starter / pump bus. “Rails off the ECU” means **sourced from the pack through island PSUs**, never tapped raw from ECU power.

---

## 3. Pack voltage (B3 / B11) — placeholder stamp

**Status:** **PLACEHOLDER — needs platform battery SKU confirm** before DC-DC PNs go BUY.

| Case | Vnom | Vmin (cut) | Vmax (chg) | Use |
|------|------|------------|------------|-----|
| **A — Shared jet pack (3S LiPo)** | 11.1 V | ~9.0 V | 12.6 V | Matches JetCat “LiPo 3s” recommendation |
| **B — Shared jet pack (4S LiPo)** | 14.8 V | ~12.0 V | 16.8 V | Still inside ECU 10–30 V; more headroom for island |
| **C — Separate avionics pack (preferred if mass allows)** | **4S–6S** (14.8–22.2 V nom) | per BMS | per BMS | Cleanest isolation; island 9–24 V carrier friendly |

**Airframe recommendation (pending builder confirm):**
- Prefer **Case C** (separate avionics pack) or at least **Case B (4S)** for Orin island input (carriers typically 9–24 V; 3S under load can brown out).
- If forced to **Case A (3S shared)**: island input DC-DC must be specified for **9–13 V** with starter-transient immunity, and isolation from ECU remains mandatory.
- Until SKU lands: BOM rows for isolated 12 / 5 / 3.3 V and island input DC-DC stay **HOLD**.

**Thermal note (Integration):** V1 thermal ADK **DEFER** — do **not** size island PSU for ADK heater/PoC in V1.

---

## 4. Isolated power architecture

```
Pack (avionics or shared) ──► Island input DC-DC / filter ──► Orin carrier (9–24 V class)
         │
         ├── Isolated 12 V ──► EO (Basler GMSL2)     size ≥5 W
         ├── Isolated 5 V  ──► misc / IMU if needed
         └── Isolated 3.3 V ──► Boson/misc if fitted   size ≥2 W (V1 EO-primary; thermal defer)
         
Jet ECU / starter / pump ──✗── no direct tap to cam or Orin logic rails
```

| Rail | Load budget (Integration) | Isolation |
|------|---------------------------|-----------|
| 12 V EO | ≥5 W (~0.45 A) | Required vs ECU/ESC |
| 3.3 V | ≥2 W Boson-class (V1 may unused) | Required |
| 3.3/5 V IMU+GNSS+misc | <3 W | Required |
| Island input | 15–30 W sust / **60–80 W peak** + margin | Filter + star ground |

Star-ground at island; single-point return; no ground loops through turbine structure.

---

## 5. Nose + payload bay mechanical

### 5.1 Nose envelope (from Nose ICD V1)

| Parameter | Floor | Status |
|-----------|-------|--------|
| Bay depth (firewall → window inner) | ≥ **110 mm** | Builder mold stamp open |
| Clear window Ø | ≥ **40 mm** | Builder mold stamp open |
| Window face | **Flush**, no step | Locked |
| Inlet keep-out | ≥ **1.5×** inlet lip Ø from window CL | Locked |
| EGT keep-out | No nose kit aft of turbine mount plane; harness ≥ **50 mm** from EGT path | Locked |

### 5.2 EO + IMU bracket

- Global-shutter EO (Basler GMSL2 class) + Nav IMU on **one rigid bracket**.
- Lever arm optical center → IMU origin = calibration item (mm, body frame).
- **Elastomeric isolators → airframe hardpoints only.**
- **REJECT** hard-mount to **turbine / engine mount**.
- Optical axis ≈ aircraft +X (±1° before cal).

### 5.3 Payload / compute bay

- Host **Forecr DSBOARD-ORNX + DSADDON-GMSL-NX** island (primary).
- Cooling path for Orin; log SoC/GPU temp (hardware owns sensors; airframe must not block ducts).
- Nose kit mass logged **separately** from island dry mass.

---

## 6. FAKRA bulkhead + cable runs

| Segment | Connector | Rule |
|---------|-----------|------|
| Island | **Quad Mini-FAKRA** (DSADDON default) | Aft of bulkhead |
| Airframe bulkhead | **Standard FAKRA** (one sealed pass-through per EO) | **FIRM ACCEPT** |
| Nose EO | FAKRA → camera | Strain relief; no pull on bracket |
| MMCX | Island-only option | **REJECT** bare MMCX in nose bay |
| CSI-2 | — | **REJECT** for nose (≤0.5 m on-carrier only) |

Route GMSL2 / power away from ignition, EGT, and high-current starter leads. Exact cam→Orin length: **builder measure** (open).

---

## 7. RF / antenna

| Antenna | Placement | Status |
|---------|-----------|--------|
| RFD900-class C2 (900 MHz) | **Wingtip only** | Locked; matches TELEM1 / comms |
| Forbidden | Nose, exhaust, EGT bay | Locked |

Do not change wingtip planform beyond the antenna hole / fairing needed for RFD900.

---

## 8. CG / mass budget — placeholders

**Fuel mass frozen.** Payload additions are logged; do not “make room” by cutting fuel.

| Line item | Mass (g) | Status |
|-----------|----------|--------|
| Orin NX SOM | ~28 (forum cite) | Estimate |
| Forecr DSBOARD-ORNX | 85 | Doc |
| DSADDON-GMSL-NX | **UNKNOWN** | Blocks firm island close |
| Cooler | TBD | Prefer light (e.g. ~33 g class if thermal OK) |
| Sync mezz (if any) | ~8–15 est | Estimate |
| **Island dry (known so far)** | **113 + addon + cooler** | **CONDITIONAL ACCEPT** class; soft ceiling **>1000 g → AUW/CG waiver** |
| Target before harness | ≤ **500–800** | Integration / airframe |
| Nose kit (window+bracket+EO+IMU+isolators) | **TBD — separate line** | Weigh at build |
| Isolated DC-DC set | TBD | After pack V |
| Harness (FAKRA + power + SPI) | TBD | Measure |
| RFD900 + wingtip ant | ~20–40 + ant | Class |
| **REJECT** | Miivii APEX ~**1800** | Mass FAIL |

**CG placeholder:** place island near existing equipment bay / CG station; keep nose kit light and on centerline. Builder supplies empty weight + CG envelope before any >1.0 kg island waiver.

---

## 9. Builder do-not-move (post-cal)

After extrinsic / lever-arm cal is signed:

1. Do not re-clock EO relative to IMU on the bracket.  
2. Do not change window↔sensor stack without re-cal.  
3. Do not move isolator hardpoints or change durometer without re-cal.  
4. Do not let FAKRA/power strain the bracket.  
5. Any physical move = **new cal ID**.

---

## 10. Open items (airframe)

| ID | Item | Blocks | Owner |
|----|------|--------|-------|
| B3/B11 | Pack Vnom/Vmin/Vmax or battery SKU | DC-DC PNs | platform + airframe + Integration |
| B4 | Addon + cooler + PSU weigh | CG close | hardware → airframe |
| — | Mold nose depth / window Ø | Window buy | Builder |
| — | Cam→Orin cable length | Harness cut | Builder |
| B7 | Lens barrel vs nose depth | EO lens | Tracking + airframe |
| — | Isolator SKU | Vib | hardware + airframe |

---

## 11. Stamp summary

| Interface | Stamp |
|-----------|--------|
| Isolated cam/Orin rails ≠ jet ECU | **PASS** (required) |
| Mini-FAKRA → FAKRA bulkhead | **FIRM ACCEPT** |
| CSI nose | **REJECT** |
| EO+IMU isolators ≠ turbine mount | **PASS** (required) |
| Wingtip-only 900 MHz | **PASS** (required) |
| Forecr 85 g board mass class | **CONDITIONAL ACCEPT** (addon+cooler pending) |
| Miivii 1.8 kg brick | **REJECT** |
| Pack V / DC-DC PNs | **HOLD** (placeholder table in §3) |
| Nose mold dims | **OPEN** |

**Next:** builder confirms pack SKU (close §3); hardware returns addon+cooler grams (close §8); then nominate isolated DC-DC PNs.
