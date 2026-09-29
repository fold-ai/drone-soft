# Soft-land profile (TEST_RECOVER)

**Owner:** Navigation  
**Gate:** `ap_mission_sm_land_soft_active()` — ABORT submode when `TEST_RECOVER=1` after CLOSE miss.  
**No new MISSION_STATE** (BOOT..FTS 0–6 unchanged).

## Sequence
1. **Bleed** — cap airspeed (~18 m/s), wings level / hold heading  
2. **Descend** — NED `vz = +1…+2 m/s`, hold heading  
3. **Disarm** — on WoW or timeout (~45 s) → `request_disarm` for mavlink_bridge  

When `land_soft_active == false`, profile resets and publishes nothing.
