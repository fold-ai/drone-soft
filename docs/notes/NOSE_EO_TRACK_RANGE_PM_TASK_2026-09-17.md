# PM task — Nose EO track + range harden (2026-09-17)

Status: **channel work order** — improve what is already on Mac `actprove-drone`; keep DEMO labels.

## Baseline (user Mac, reviewed PASS)
- Continuous DEMO accompany after Capture: CSRT→KCF→MIL→TMPL, browser JPEG → `/track/*`
- Monocular EST range: horizontal span + Known width / HFOV / Ref. range calib UI
- Adaptive EMA range; reject clipped / too-small boxes
- Host tests: `tools/test_nose_eo_tracking.py` **5/5 PASS**
- Docs: `tools/README_NOSE_EO_DEMO.md`, `docs/notes/CLICK_LOCK_NOSE_EO.md`

## Goal A — Improve what you own
Harden track hold + EST usefulness without claiming radar/LiDAR accuracy. UI/logs stay **DEMO** until class-gated Lock PASS.

## Goal B — High closing-speed calib (PM priority)
**Problem:** at fast approach / fast lateral motion the box loses lock and EST blows up (bottle lab now; Shahed-class later).

Deliver a **calib + hold procedure** so that under rapid range change the tracker stays on target longer and EST does not spike/lag into nonsense.

### Acceptance (demo lab, Mac GoPro Nose EO)
1. Static Capture → move target slowly: box follows ≥5 s, EST continuous, CLOSING when approaching.
2. **Fast approach** (operator walks target toward camera quickly, or camera toward target): track hold ≥3 s OR controlled re-acquire <0.5 s without operator re-click; EST must not jump by >2× in one frame (filter already helps — prove with log).
3. Document calib knobs: Known width, HFOV (Calibrate FOV at known range), CSRT params / search window, JPEG size/Hz, YOLO re-acquire ROI, lost fail count.
4. Add or extend host test for “fast translating + growing bbox” (closing simulation).
5. No buys. No claim of flight Lock. Class Lock still needs `label_batch_v1`.

## Channel ownership
| Channel | Own |
|---------|-----|
| perception | Class vs DEMO boundary; YOLO re-acquire; future Orin hot-path notes; Known-width defaults per class |
| Tracking | Hold / coast / re-acquire policy; measurement_epoch; high-speed miss model |
| System stack | GCS calib UI persistence; HUD EST/CLOSING; DEMO labels; sync Mac↔docs |
| test | Lab card + pass/fail for slow vs fast approach; log criteria |
| hardware | Cam A 40 mm / GoPro demo FoV notes; what breaks at jet closing (exposure, blur) |
| Integration | When demo track may feed SEARCH bias vs never Lock; profile hooks |
| Navigation | Consume EST only as soft cue if at all — not guidance truth until stamped |

## Explicit non-goals
- Radar/stereo range
- Class-gated Lock PASS without real boxes
- Carrier / FAE buy acceleration
