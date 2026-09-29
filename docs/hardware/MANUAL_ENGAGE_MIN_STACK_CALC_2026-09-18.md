# Minimal stack calc — climb → Engage → defeat → LAND (PM 2026-09-18)

**Primary goal only.** Everything else (Shahed Lock, Forecr, sync mezz, FIM SA, Basler Cam A, jet 800 km/h) = out.

**Mass freeze (user skipped A/B widget):** **A — companion island ≤1.5 lb (~680 g).** Cube+Here3+ELRS RX = FC island on airframe, **not** in 680 g.

---

## 1. Mission loop (what HW must enable)

| Step | Who flies | Radio | Compute |
|------|-----------|-------|---------|
| 1 Climb / position | Pilot sticks | **ELRS** → Cube | Orin idle |
| 2 Engage | Rising edge **CH7** | ELRS → Cube → Orin `RC_CHANNELS` | Orin ARM_SEARCH |
| 3 Detect / Lock / CLOSE | Autopilot GUIDED | Orin↔Cube **TELEM2 UART**; RFD telem optional | Orin YOLO `test_drone` + TrackMsg |
| 4 Defeat | Contact or soft-kill ≤15 m | — | Nav scores terminal |
| 5 Land | LAND if TEST_RECOVER else RTL | ELRS abort always live | Cube LAND/RTL |

**Cloud (Vercel/Supabase):** logs/training only — **0 g, 0 C2.**

---

## 2. Hardware bill — MUST HAVE

### Airframe side (you have / FC island — outside 680 g)
| # | Item | Role | Est. mass | Notes |
|---|------|------|----------:|-------|
| 0 | Your interceptor + propulsion | Fly | — | Given |
| 1 | **Cube Orange+ + carrier + power module** | FC ArduPilot | ~70–120 g | BUY if missing |
| 2 | **Here3+** | Cube GPS | ~40–60 g | Dedicated — no Orin share |
| 3 | **ELRS RX** on air | Manual + Engage CH7 + abort | ~5–15 g | |
| 4 | Servos / control surfaces (elevon, aileron, elevator, rudder as applicable) | Attitude control | airframe | **VERIFY; BUY is BLOCKED on geometry/torque calculation** |
| 4a | External redundant servo power + independent packs | Power servo rail | airframe | **Required: Cube does not power servos** |
| 4b | Horns, pushrods, ball links/clevises, hinges, mounts, locking hardware | Mechanical control path | airframe | VERIFY/BUY; no generic rear-panel assumption |

### Ground
| # | Item | Role |
|---|------|------|
| 5 | **ELRS TX** (TX16S MKII ELRS or radio+Ranger) | Climb + Engage switch + abort |
| 6 | **RFD900x-US ground modem + FTDI USB + ant** | Telem / ABORT / optional cue from laptop |
| 7 | Laptop + ACTPROVE GCS or Mission Planner | Monitor; **not** required for Engage edge |

### Companion island (≤680 g — WEIGH before fly)
| # | Item | Role | Mass (g) | Keep? |
|---|------|------|---------:|-------|
| 8 | **Orin NX 16GB** SOM | Detect / track / CLOSE cmds | 28 | YES |
| 9 | **Bootable light carrier** (Seeed/dev/Auvidea-class) | Power Orin now | 80–200 | YES — not Forecr |
| 10 | Cooler / heatsink | Thermal | 30–50 | YES |
| 11 | **Interim light EO** USB3/CSI GS preferred | See target | 40–120 | YES |
| 12 | Lens ~6–12 mm (~20–40° HFOV) | Hunt FoV | 20–80 | YES |
| 13 | **RFD900x air** + short ants | C2 telem | 40–60 | YES |
| 14 | TELEM2 UART cable (Holybro 1186 / Digi-Key GHR) | Orin↔Cube | ~5–15 | YES |
| 15 | TELEM1↔RFD cable | Often in RFD bundle | ~5–10 | YES |
| 16 | NVMe (small) | Logs/weights | 10–20 | YES |
| 17 | 3D mount + island cables | Hold EO | 30–60 | YES |
| | **SUM (no FIM)** | | **~270–550** | **FIT under 680** |
| — | FIM-2410 air | SA video | +93 | **CUT** for first Engage |
| — | Basler + 40 mm | Lock optic | +185–285 | **LATER** upgrade |

### Target + range
| # | Item | Role |
|---|------|------|
| 18 | **Expendable test drone** (known size + photos) | Only kill class |
| 19 | Closed range + geo fence | TEST |
| 20 | Batteries / packs for both craft | Flight time |

---

## 3. Explicitly NOT for this first gate

| Cut | Why |
|-----|-----|
| Forecr DSBOARD | FAE; interim carrier boots |
| Sync mezz / PPS Lock PASS | Soft ts OK for test_drone gate |
| Basler a2A1920 + 40 mm | Mass + lead; upgrade later |
| FIM-2410 | Mass; SA ≠ kill |
| Thermal / FTS live RF | Out of scope |
| Vercel in loop | Never C2 |
| Shahed weights | Wrong class |

---

## 4. Money / order order (US)

1. **IR-LOCK / equiv:** RFD900x-US bundle + Cube Orange+ (+carrier/PM) + Here3+  
2. **Amazon:** ELRS TX path + RX  
3. **Digi-Key:** UART contacts if Holybro 1186 OOS  
4. **Orin NX 16GB + any bootable carrier** + small NVMe + cooler  
5. **Interim GS USB3/CSI cam + 6–12 mm lens** (≤150 g cam+lens)  
6. **Test drone** + markings  

Refs: `BUY_THIS_WEEK_US_2026-09-18.md`, `MANUAL_ENGAGE_15LB_MASS.md`.

---

## 5. Software that must exist (paired with HW)

- ELRS CH7 → Integration `engage_armed` → Tracking SEARCH  
- YOLO class **`test_drone` only** (people rejected)  
- TrackMsg → Nav GUIDED CLOSE  
- Soft-kill ≤15 m or contact → **LAND** if TEST_RECOVER else RTL  
- ELRS abort always overrides  

---

## 6. Pass criteria (this goal)

**PASS:** Manual climb → Engage → Lock `test_drone` → CLOSE → defeat (contact or ≤15 m) → LAND/RTL. Island weighed ≤680 g. ELRS abort proven before CLOSE.

**FAIL:** No Orin in air; COCO/person Lock; waiting Forecr/Basler/FIM; cloud C2.

---

## PM stamp

- Mass **A** frozen (companion only).  
- Hardware stack above = **minimum complete** for user goal.  
