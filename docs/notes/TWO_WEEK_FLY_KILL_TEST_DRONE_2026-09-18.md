# 2-week goal: fly interceptor + defeat expendable TEST drone (PM 2026-09-18)

**Goal:** User airframe flies, sees a **dedicated test UAV** (not Shahed), guides, and **physically defeats** it (ram / expendable collide) **or** scored soft-kill if range safety forbids contact.

**Out of scope for 2 weeks:** Shahed class Lock PASS, Forecr FAE carrier, sync mezz NRE, AIRFRAME_JET 700–800 km/h, combat FTS live RF (name holder ASAP).

**Doctrine for this gate:** `TEST_GEOMETRY` / test-target class → Lock on **test_drone** class only → CLOSE. Prefer **commanded collide on expendable** only on closed range with ELRS abort. If safety forbids contact: CLOSE to ≤X m + scored kill (define X with TEST).

---

## A. Hardware — MUST BUY / HAVE (2 weeks)

### A0 — Already assumed
- [ ] Interceptor airframe + propulsion (user “маю дрон”)
- [ ] Power architecture that can run Cube + radios + Orin on bench/flight (even ugly interim)

### A1 — Tier A COTS (order this week — US)
| Item | Role | Buy where |
|------|------|-----------|
| RFD900x-**US** FCC **bundle** (2 modems + ants + FTDI USB) | C2 air↔GCS | IR-LOCK / OneDrone |
| Cube Orange+ **+ carrier board + power module** | FC ArduPilot | IR-LOCK / SpektreWorks |
| Here3+ (or Cube kit GPS) | Cube EKF — dedicated | IR-LOCK / Amazon |
| ELRS: TX16S MKII ELRS **or** radio + Ranger + **≥1 RX on air** | Pilot abort / override | Amazon / RMRC |
| UART TELEM2↔Orin (Holybro 1186 or Digi-Key GHR-06V-S + contacts) | Companion link | Digi-Key if 1186 OOS |
| TELEM1↔RFD cable | Often in RFD bundle | — |

### A2 — Companion + eye (required for “see & kill”, not just RC fly)
| Item | 2-week path | Notes |
|------|-------------|-------|
| **Jetson Orin NX 16GB** (module) | BUY | Digi-Key/Arrow/Seeed — check qty |
| **Orin carrier that boots NOW** | BUY interim | If Forecr FAE blocked: use **any bootable Orin NX carrier** you can get in days (dev kit / Seeed / Auvidea-class) for TEST only — **not** flight Lock PASS stamp |
| **Flight EO** | Prefer Basler a2A1920-168mgm + 40 mm if stock/shipping ≤2 wk | If lead >2 wk: **interim USB/CSI cam on Orin** for test-drone only (DEMO-grade timestamps OK for this gate, **not** PPS Lock PASS) |
| Interim EO mount | 3D-print / clamp | CNC DEFER |
| NVMe on Orin | BUY | logs + weights |
| Bench PSU / air packs as needed | BUY | |

### A3 — Target + safety
| Item | Role |
|------|------|
| **Expendable test drone** (known size, class photos for training) | The only kill target |
| Bright/unique markings optional | Helps detection |
| ELRS abort proven on ground | Before any CLOSE |
| Range / geo fence plan | TEST owns |
| FTS | Ideal; if unnamed, **ELRS-only abort** + low-energy TEST profile — document residual risk |

### A4 — Explicitly NOT required for this 2-week gate
Forecr FAE PASS · sync mezz · Cam A Lock PASS stamp · Shahed labels · thermal · jet 800 km/h profile

---

## B. Software — MUST DO (2 weeks)

### B1 — Ground / RF
1. Live `gcs_rfd_bridge` (replace dry-run) — HEARTBEAT, OrbitCue/WORK, ABORT
2. Mission Planner or ACTPROVE GCS talking RFD
3. Cube params: TELEM1 RFD, TELEM2 Orin, ELRS RCIN, failsafe RTL/ABORT map
4. FS-03 companion HB → RTL (already designed — enable on air)

### B2 — Perception (test drone, not Shahed)
1. Collect **≥200–500** images of **your test drone** (multi altitude/aspect/background)
2. Label Studio tight boxes + **negatives** (people, ground clutter)
3. Train YOLO (or current stack) class `test_drone` only
4. Export → Orin runtime; **reject** person/COCO as Lock
5. Demo GoPro path stays lab; flight uses Orin EO

### B3 — Tracking / Nav / Integration
1. Track hold on `test_drone` (high-speed hold policy OK for ~50 m/s class)
2. After Lock: GUIDED CLOSE via `orbit_plan_to_guided` / TEST_GEOMETRY profile
3. Mode machine: SEARCH (cue optional) → Lock(test_drone) → CLOSE → BDA/score
4. EST range soft only — do not steer on EST alone
5. Logs: jpeg_hz / track / mavlink for TEST card

### B4 — TEST scoring
1. Update card: **FLY + DEFEAT test drone** criteria (contact **or** ≤X m soft-kill)
2. Gating flights: (a) RC-only smoke (b) RFD+GUIDED no target (c) vision Lock on ground static (d) air vs test drone

---

## C. Week plan (aggressive)

| Days | Focus |
|------|-------|
| 1–3 | Order Tier A + Orin + interim cam; Cube+RFD+ELRS bench HEARTBEAT |
| 3–7 | Photos/labels/train `test_drone`; UART Orin↔Cube; bridge live |
| 7–10 | Ground: Lock on parked/hovering test drone → GUIDED command smoke |
| 10–14 | Flight: interceptor RC smoke → cue flight → **vs expendable test drone** CLOSE |

---

## D. Honest pass/fail

**PASS (2-week):** interceptor airborne under Cube+RFD+ELRS; Orin sees `test_drone`; Lock; CLOSE; **physical defeat or agreed soft-kill**.

**FAIL / slip:** no Orin in air; model only COCO; no ELRS abort; trying Shahed weights; waiting on Forecr/mezz.

