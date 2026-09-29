# safety_gates — V1 SAFETY mission state machine

**Owner:** SAFETY  
**Source of truth:** `V1_PLAN.md` §6  
**States:** BOOT, SEARCH, LOCK, CLOSE, ABORT, RTB, FTS (`ap_mission_state_t` / `MISSION_STATE`)  
**Mission:** see / hold / commanded miss — **NOT** kill. No terminal stick. No combat-intercept.

Ground cmds (exact strings): `WORK`, `ABORT`, `RTB`, `FTS`, `TARGET_DESTROYED`, `MISS/REATTACK`.

## Layout

| Path | Role |
|------|------|
| `include/safety_gates/mission_sm.h` | Public C11 API |
| `src/mission_sm.c` | State machine |
| `tests/test_mission_sm.c` | SAFETY acceptance unit tests |
| `CMakeLists.txt` | Library target `safety_gates` + test |
| `docs/forbidden_v1.md` | Forbidden V1 behaviors |

Uses existing enums/macros from `interfaces/mavlink/mission_state.h` and `mavlink_v1_cmds.h` (do not fork enums). Enum stays **BOOT..FTS = 0..6** — no `LAND_SOFT` as enum 7.

## Inputs (`ap_mission_sm_inputs_t`)

| Field | Role |
|-------|------|
| `heartbeat_ok` | Orin HEARTBEAT this tick |
| `bit_ok` | FCS/sensors BIT OK |
| `corridor_loaded` | Engagement corridor loaded |
| `track_box_valid` | Stable track + box |
| `geometry_ok_for_close` | OK to enter CLOSE |
| `miss_hold_done` | Commanded-miss hold complete |
| `hard_fail` | Irrecoverable → FTS |
| `operator_lock_request` | In **SEARCH** only: with corridor + box → **LOCK** (manual path; auto path still uses `track_box_valid` alone) |
| `test_recover` | Latched at **WORK→SEARCH** as `test_recover_armed`; after CLOSE miss → ABORT submode **LAND_SOFT** |

## ABORT submode: LAND_SOFT

When `test_recover` was armed before WORK and CLOSE finishes miss hold:

- Enter **ABORT** with `land_soft_active=1` (reason `"LAND_SOFT"`). Query via `ap_mission_sm_land_soft_active()`.
- Auto **lost-link does not** → RTB while LAND_SOFT (soft-land continues).
- Explicit **RTB** cmd still overrides → RTB.
- `hard_fail` / **FTS** cmd → FTS as usual.
- Cleared on leave ABORT / reset / WORK→SEARCH.

With `test_recover=0` (expendable profile): existing behavior — CLOSE miss → plain ABORT; lost-link → RTB.

See also `docs/notes/MANUAL_LOCK_AND_TEST_LAND.md`.


## Operator BDA (ground)

Operator battle-damage assessment uplink (see `docs/notes/GROUND_EO_CUE_AND_BDA.md`). Exact strings only — no aliases. No separate `MISS` or `REATTACK` wire names.

| Cmd | USER_1 param1 | SM effect |
|-----|---------------|-----------|
| `TARGET_DESTROYED` | `AP_UPLINK_TARGET_DESTROYED` (=3) | Clear engagement → **RTB** (always; never LAND_SOFT) |
| `MISS/REATTACK` | `AP_UPLINK_MISS_REATTACK` (=4) | Clear CLOSE/LOCK engagement → **ABORT** (`engagement_cleared`); new **WORK** may re-enter SEARCH (re-cue) |

Accepted in **SEARCH / LOCK / CLOSE / ABORT**. Ignored in **BOOT** and latched **FTS**. No-op ignore in **RTB**.

### TEST_RECOVER / LAND_SOFT interaction (V1 freeze)

- **`TARGET_DESTROYED` always → RTB** — even if `test_recover_armed`. Operator BDA destroyed prefers cruise RTB (high-speed miss-loop default). Does **not** enter LAND_SOFT.
- **LAND_SOFT** applies only to the **auto** post-CLOSE `miss_hold_done` path when `test_recover` was armed at WORK→SEARCH.
- `MISS/REATTACK` → plain ABORT (cleared), never auto-kill / never auto-FTS / never auto-RTB.

Still forbidden: terminal stick, auto-kill without operator BDA, combat-intercept state.

## Default timeouts

| Symbol | Default | Meaning |
|--------|---------|---------|
| `T_boot` | **30 s** | BOOT without BIT OK → ABORT |
| `T_search` | **60 s** | SEARCH without lock → ABORT |
| `T_coast` | **2 s** | Lost box in LOCK/CLOSE → SEARCH |
| `T_link` | **3 s** | No Orin HEARTBEAT → **RTB** (`AP_LOST_LINK_RTB_S`; never auto-FTS; LAND_SOFT exception) |

## How to use

```c
#include "safety_gates/mission_sm.h"

ap_mission_sm_t sm;
ap_mission_sm_init(&sm, NULL);           /* defaults */
ap_mission_sm_handle_command(&sm, "WORK");
ap_mission_sm_inputs_t in = {
  .heartbeat_ok = 1, .bit_ok = 1, .corridor_loaded = 1,
  .test_recover = 1,                 /* arm soft-land before WORK */
};
ap_mission_sm_tick(&sm, now_s, &in);
ap_mission_state_t st = ap_mission_sm_state(&sm);
int soft = ap_mission_sm_land_soft_active(&sm);
ap_mission_sm_fts_outputs_t fts;
ap_mission_sm_fts_outputs(&sm, &fts);    /* only asserted in FTS */
ap_mission_sm_reset(&sm);                /* sim/bench exit from FTS latch */
```

## Build & test

### CMake (preferred)

```bash
cd onboard/safety_gates
cmake -S . -B build && cmake --build build && ./build/test_mission_sm
```

### gcc one-liner (if cmake missing)

```bash
cd onboard/safety_gates
gcc -std=c11 -Wall -Wextra -O2 \
  -Iinclude -I../../interfaces/mavlink \
  -o test_mission_sm src/mission_sm.c tests/test_mission_sm.c \
  && ./test_mission_sm
```

## Forbidden V1 (summary)

See `docs/forbidden_v1.md`. In short: no explosive FTS, no combat-intercept, no SEARCH without WORK+corridor, lost-link→RTB only (LAND_SOFT hold exception), WORK ignored in LOCK/CLOSE.

## Legacy note

Older C++ sketches under `include/safety_gates/*.hpp` and `src/state_machine.cpp` are **not** the V1 deliverable; use `mission_sm.[ch]`.
