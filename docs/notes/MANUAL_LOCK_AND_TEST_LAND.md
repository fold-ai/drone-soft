# Manual Lock + Test soft-land (PM design, 2026-09-16)

## A) Manual lock (trail / nose camera)

**Goal:** Operator can force Lock on a chosen object when auto detect is wrong or target is hard — without waiting only for YOLO.

### Preferred GCS (V1 test)
1. **Tablet/laptop GCS** on RFD900 C2 (QGC/custom) **plus** near-field video on second RF (or USB/Ethernet if iron-bird).
2. Video overlay from EO (nose): tap target → send **pixel/box designate** to Orin.
3. Optional map: tap approximate bearing only as **cue** (SEARCH bias), not as Lock — Lock always needs image box or track.

### Wire (no new dialect if we can avoid it)
| Step | Who | What |
|------|-----|------|
| Tap on video | GCS app | Box in full-frame pixels + timestamp |
| Uplink | GCS→Orin | Prefer existing thin C2: e.g. `NAMED_VALUE_INT`/`STATUSTEXT` short code **or** small JSON over companion link if bandwidth allows; longer-term custom msg — **not** video on RFD900 |
| Orin | Tracking | Seed/force track from designate; `class_id=target_uav` if operator confirms type, else `operator_force` flag |
| Safety | SM | New input `operator_lock_request` → may enter **LOCK** if box valid + corridor loaded + WORK already issued |

**Modes**
- `AUTO_LOCK` — detector class gate only (default).
- `MANUAL_LOCK` — operator designate required / may override class.
- `SEMI` — detector proposes, operator confirms (tap = accept).

Trail ops: operator stands aside with tablet; drone on trail/runway; nose cam looks forward; WORK → SEARCH; tap when target visible → LOCK → CLOSE miss.

### Do not
- Lock from map pin alone (no range/bearing without sensors).
- Stream HD on RFD900.
- Skip corridor / WORK.

---

## B) After engagement — test soft-land (recoverable)

**Production later:** airframe often expendable after hard intercept.  
**V1 / range tests with fragile surrogate drones:** after **commanded miss complete** (or soft contact test), auto **soft land** so airframe is recoverable.

### New post-CLOSE behavior (test profile only)
Add mission flag `TEST_RECOVER=1` (param / GCS):

After CLOSE finishes miss geometry (or ABORT after miss done):
1. Clear engagement (no new hunt).
2. Enter **`LAND_SOFT`** (can map to existing ABORT→special or new state — prefer **new state or ABORT submode** to avoid fighting RTB).
3. Profile (tune later):
   - Cap airspeed → slow (e.g. approach speed class)
   - Command gentle descent rate (e.g. −1…−2 m/s)
   - Wings-level / hold heading or into wind
   - Disarm on weight-on-wheels / low alt+low speed timeout
4. If lost-link during LAND_SOFT → still try soft land if energy OK; else FTS only on hard fail / FTS cmd.
5. If `TEST_RECOVER=0` (expendable profile): after CLOSE → RTB or safe ditch per range rules — **not** terminal stick.

### Why not plain RTB?
RTB flies home at cruise — bad after close pass / damaged prop. Soft-land = **bleed energy in place / short final**.

### Safety
- LAND_SOFT only if `TEST_RECOVER` armed from GCS before WORK.
- Never auto-LAND_SOFT in “expendable” profile.
- FTS still operator / hard-fail only.

---

## C) Implementation owners
| Piece | Owner |
|-------|--------|
| Tablet tap → box uplink | comms + GCS app |
| Tracking seed from designate | Tracking |
| `operator_lock_request` in SM | safety |
| LAND_SOFT profile / PX4 mode | Navigation + airframe-iface + safety |
| Test checklist | test |

---

## SAFETY freeze (accepted 2026-09-16)

### A) `operator_lock_request`
- New SM input (edge/pulse). **SEARCH→LOCK** only if: WORK already issued (in SEARCH), `corridor_loaded`, and `track_box_valid` from designate (image box — **never map pin alone**).
- AUTO/SEMI/MANUAL modes live in Tracking/GCS; SM does **not** own class gates — only lock request + box validity.
- Does **not** bypass WORK or corridor. Ignored in BOOT/ABORT/RTB/FTS/CLOSE; in LOCK = no-op.

### B) `LAND_SOFT`
- Prefer **ABORT submode** (not new `MISSION_STATE` enum) so NAMED_VALUE_INT 0–6 stays frozen with comms. Expose `land_soft_active` + reason string / separate flag for logs.
- Enter only if `TEST_RECOVER=1` armed **before WORK**. After CLOSE `miss_hold_done` (or ABORT after miss done): clear engagement → ABORT+LAND_SOFT (**not** RTB).
- `TEST_RECOVER=0`: CLOSE→ABORT then normal RTB/range ditch — still **no terminal stick**.
- Lost-link during LAND_SOFT: **do not** force RTB; continue soft-land if energy OK; FTS only on hard_fail / FTS cmd (exception to global 3s→RTB).
- Nav owns speed/descent/disarm profile; SM only gates the mode.

### C) Implementation
- SAFETY writing `/workspace/actprove-drone/onboard/safety_gates/` (box write, no PR); ping when tests green.
