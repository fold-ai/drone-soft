# 2-week test — NRE descope (PM 2026-09-18)

User constraint: testing in ~2 weeks; custom PCB/CNC will not finish.

## Decisions
1. **sync mezz NRE → DEFER** for 2-week gate. Flight Lock still needs PPS/FSYNC/DRDY (FAE + mezz later). Soft host timestamps remain **REJECT** for Lock PASS — so 2-week = RF/cmd/DEMO, not class Lock flight PASS.
2. **Orin↔Cube UART / bench harness → BUY COTS** (CubePilot JST-GH / Digi-Key). Not NRE.
3. **EO+IMU mount → interim 3D-print / clamp** for bench or GoPro DEMO. Flight rigid+isolator CNC **DEFER**.
4. Unblock 2-week with **RFD ground + ELRS + Cube + UART + GCS bridge**.

## Channel actions
- hardware: mark mezz/mount as post-2-week; list COTS cable PNs
- Nav/Integration: 2-week success = TELEM HEARTBEAT + cue/WORK dry→live; not Lock PASS
- test: RF card scored without mezz
- perception: keep labeling; Lock PASS not on 2-week critical path
