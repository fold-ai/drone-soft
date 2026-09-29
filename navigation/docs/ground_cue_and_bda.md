# Navigation — ground EO cue acceptance + BDA exit paths

**Owner:** Navigation  
**Upstream spec:** `docs/notes/GROUND_EO_CUE_AND_BDA.md` (PM freeze)  
**Language:** English (engineering ICD)

## 1. Problem Nav owns

V1 often has **no radar**. A ground EO cue may arrive as **bearing-only** (az/el LOS) before the nose camera has a track. After CLOSE, the operator marks BDA; Nav must fly the correct **exit path** and, on miss, accept a **re-cue** heading.

## 2. Ground cue acceptance (bearing-only OK)

### 2.1 Accepted cue forms

| Field | Required? | Notes |
|-------|-----------|-------|
| `t_pps` | Yes | Cue epoch (GCS/Orin PPS domain when available) |
| `bearing_az_ned_rad` | Yes | Horizontal LOS from interceptor (or geo-derived) |
| `bearing_el_ned_rad` | Preferred | May be 0 / unknown if ground cam has no range |
| `range_est_m` | Optional | **NaN allowed** — bearing-only is valid until nose acquire |
| `cue_source` | Yes | `GROUND_EO` \| `NOSE_EO` \| `OPERATOR` |
| `cue_quality` | Yes | 0..1; Nav ignores cue if `< cue_quality_min` (default 0.3) |
| `operator_lock_request` | Safety | SM owns SEARCH→LOCK gate; Nav only steers |

### 2.2 Nav policy until nose acquires

1. **Accept bearing-only cue** as SEARCH / pre-LOCK heading bias (command yaw + horizontal vel toward LOS).  
2. **Do not** invent range, PIP, or kill geometry from a ground cue alone.  
3. When Tracking publishes a **nose** `TrackMsg` with `lock_quality ≥ 0.5` and `state=Confirmed|Coast`, **handoff**: nose track supersedes ground cue for CLOSE guidance.  
4. Map-pin-only cue (**no** image box / bearing) → **REJECT** (same as Safety freeze).  
5. Lost cue before nose acquire → hold last commanded heading ≤ `cue_coast_s`, then idle (Safety may ABORT/RTB).

### 2.3 Re-cue after MISS / REATTACK

On operator `MISS` / `REATTACK` (Safety clears CLOSE / allows new WORK):

- Nav **drops** active CLOSE / miss geometry.  
- Accepts a **new** ground (or nose) cue the same way as §2.1.  
- Re-enters heading bias toward new bearing; CLOSE guidance only after SM returns to LOCK/CLOSE with a usable track (or high-quality cue + Safety policy).  
- Does **not** auto-declare kill or auto-re-engage without WORK / SM.

## 3. BDA exit paths (Nav)

Safety owns the mission state. Nav owns the **flight profile** once SM has selected the path.

| Operator BDA | Safety gate (typical) | Navigation action |
|--------------|----------------------|-------------------|
| `TARGET_DESTROYED` | Clear engagement → **RTB** (default) | Fly **RTB profile** (home / recovery point setpoints). |
| `TARGET_DESTROYED` (even if TEST_RECOVER armed) | SM → **RTB** | Fly **RTB** — do **not** enter LAND_SOFT on BDA-destroyed. Soft-land is only auto post-CLOSE miss. |
| `MISS` / `REATTACK` | Clear CLOSE; new WORK or RTB per test card | Stop CLOSE guidance; **re-cue heading** when new cue arrives; if SM chooses RTB without re-WORK, fly RTB profile. |
| No BDA / keep hunting forever | Forbidden | Nav will not free-hunt; no cue / no track → no CLOSE setpoints. |

### 3.1 Priority (when flags conflict)

```
if BDA == TARGET_DESTROYED:
    RtbProfile          # always — clean exit; do not fight LAND_SOFT
elif ap_mission_sm_land_soft_active():
    SoftLandProfile     # auto TEST_RECOVER after CLOSE miss only
elif SM RTB:
    RtbProfile
elif BDA == MISS/REATTACK:
    clear CLOSE; CueHeadingBias if new cue else idle
elif CLOSE && nose track usable:
    HeadingGuidance (commanded miss)
elif cue usable (SEARCH|LOCK):
    CueHeadingBias (bearing-only OK)
else:
    no guidance publish
```

### 3.2 Non-goals

- Autonomous BDA / kill declaration.  
- Radar.  
- Terminal stick / impact guidance.  
- New `MISSION_STATE` enum values (BOOT..FTS 0–6 frozen).

## 4. Code map

| Artifact | Role |
|----------|------|
| `include/navigation/ground_cue.hpp` | Cue ICD + acceptance helpers |
| `include/navigation/bda_exit.hpp` | Exit-path selector (RTB vs soft-land vs re-cue) |
| `include/navigation/rtb_profile.hpp` | RTB setpoint stub |
| `src/bda_exit.cpp` / `rtb_profile.cpp` | Stubs |
| `docs/soft_land.md` | LAND_SOFT profile (existing) |

## 5. Test hooks

Smoke covers: bearing-only cue accept/reject; `TARGET_DESTROYED` → RTB vs soft-land when `land_soft_active`; `MISS` clears CLOSE and accepts re-cue.
