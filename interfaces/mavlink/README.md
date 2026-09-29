# interfaces/mavlink — V1 constants (common.xml only)

**Owner:** #comms (+ System stack for bridge)  
**ICD:** [`docs/icd/mavlink_v1.md`](../../docs/icd/mavlink_v1.md)

No custom MAVLink dialect. Headers mirror frozen uplink names and the TELEM map for the Orin bridge / GCS tooling.

| File | Contents |
|------|----------|
| `mavlink_v1_cmds.h` | WORK / ABORT / RTB / FTS command IDs + params |
| `mission_state.h` | `MISSION_STATE` enum 0–6 |
| `telem_map.h` | TELEM1/TELEM2 roles, baud, air rate, component IDs |
| `message_table.md` | Compact message list (same as ICD §table) |
