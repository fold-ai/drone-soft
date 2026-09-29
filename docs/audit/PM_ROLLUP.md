# PM virtual audit rollup — 2026-09-16

**Airframe:** N250 jet + airframe GIVEN · **EO-only** (no thermal) · Full internals designed around them.

| Channel | Verdict | Virtual tests | Key open |
|---------|---------|---------------|----------|
| Navigation | CONDITIONAL PASS | navigation_smoke PASS | DRDY→HTE HW HOLD |
| Tracking | PASS | 23/23 PASS | — |
| Safety | GREEN (SW) | unit SM exit 0 | FTS holder blocks G2/G3 |
| Comms | PASS with OPEN | 11 PASS / 0 FAIL / 1 OPEN | Near-field video SKU; N250 TELEM confirm |
| Hardware | AUDIT IN | — | Forecr FAE; pack V; no carrier PO |
| Integration | sync PASS / buy FAIL | — | HTE blocks Integration PASS |
| Airframe-iface | AUDIT IN | — | Pack V / DC-DC PN HOLD |
| Perception | PASS with OPEN | val smoke 12 img → 18 dets | full-frame labels; no negatives; 3-ep CPU weights |
| System stack | PARTIAL | 12 PASS / 0 FAIL / 5 SKIP | soft MAVLink stub; no live BDA uplink from GCS; time_sync no smoke |
| Test consolidator | G1-SW GO / G-RANGE CONDITIONAL / G2–G3 NO-GO | see TEST_VIRTUAL_RUN.md | FTS holder unnamed |

## Software vs hardware gate
- **Software path for range-test semantics:** largely PASS (SM BDA, Nav RTB/soft-land, Tracking handoff, Comms ICD). System stack **PARTIAL** — smokes green but bridge still soft-stub / GCS not live-wired.
- **Hardware Integration PASS:** still **FAIL/HOLD** until Forecr FAE (HTE+PPS/FSYNC+mass) + pack V + FTS holder.

## Next user actions
1. Send Forecr FAE email (HTE GPIO, PPS/FSYNC names, addon/cooler mass).
2. Name pack V / battery SKU.
3. Name FTS holder.

See also: `docs/hardware/HARDWARE_STACK_N250.md`, `ASSEMBLY_PLAN_N250.md`.

## Final go/no-go (test)
- **G1-style SW:** GO
- **G-RANGE-NR-V1:** CONDITIONAL GO (needs FTS holder + live Ground EO ops)
- **G2/G3:** NO-GO (FTS holder unnamed; TELEM/HW sync not on box)
- Does **not** unlock speed while G3 red.
