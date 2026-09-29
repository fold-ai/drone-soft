# Operator-authorize kill (PM 2026-09-18)

**User CONOPS (first flights):** interceptor flies toward area; stack **detects airborne targets**; when it **fixes** one → **red box**; system **asks permission**; only after operator presses **Kill / Збити** → pursue + defeat. Not auto-CLOSE on Lock alone.

## Mode rail (updated)

```
MANUAL → TEST (pre-Engage checkout)
  → ENGAGED / SEARCH (detect airborne; class gate)
  → CANDIDATE / LOCK_HOLD (red box on fixed target; NO CLOSE yet)
  → WAIT_AUTHORIZE (GCS prompt: Kill / Abort)
  → CLOSE (only after Kill button) → LAND/RTL
```

| Phase | What happens | CLOSE? |
|-------|--------------|--------|
| SEARCH | Detect airborne (prefer `test_drone`; reject people/ground) | No |
| LOCK_HOLD | Fix track + **red HUD box** | No |
| WAIT_AUTHORIZE | Operator decision | No |
| Kill pressed | Arm prosecute | **Yes** → CLOSE |
| Abort / ELRS | Clear authorize + track as Safety | No |

**CH7 Engage** = enter SEARCH (or keep prior meaning: arm search path after TEST PASS).  
**Kill button** = separate GCS (or spare RC CH later) — **required** before CLOSE.  
TEST (CH8) unchanged. Soft-kill ≤15 m / LAND stamps unchanged.

## Product freeze

1. **Red box = proposed Lock**, not kill authorization.  
2. **Kill button = sole software arm for CLOSE** (plus Safety/TEST gates already stamped).  
3. Class gate still: Lock candidates = airborne + `test_drone` when model ready; people ≠ Lock.  
4. Photos of test UAV still required for reliable `test_drone` — until then DEMO/airborne heuristic may show red box but Kill→CLOSE stays Safety-gated / TEST-card gated.  
5. Cloud never in authorize loop — Kill is local GCS or RC.

## Ownership

| Channel | OWN |
|---------|-----|
| System stack | GCS: red box + Kill / Abort prompt; rail WAIT_AUTHORIZE |
| Tracking | Fix → red box state; no CLOSE until authorize flag |
| Integration | `kill_authorized` wire; CH7 ≠ Kill; mode WAIT_AUTHORIZE |
| Navigation | CLOSE only if `kill_authorized` |
| perception | Airborne / test_drone detect; reject people |
| safety | Kill cannot bypass abort; authorize timeout → drop |
| test | Update fly_defeat + preengage cards: Kill before CLOSE |
| comms | Authorize is local; RFD may mirror state only |

## Explicit

- Prior stamp “Lock → engage-to-kill (not operator BDA)” **superseded for this first-test profile** by **operator Kill before CLOSE**.  
- Shahed combat auto-prosecute remains future; this profile = HITL kill.
