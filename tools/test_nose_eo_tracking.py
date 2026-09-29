#!/usr/bin/env python3
"""Host tests for Nose EO DEMO track helpers — no camera / YOLO weights required.

Run:
  python tools/test_nose_eo_tracking.py
"""
from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

# Allow `python tools/test_nose_eo_tracking.py` from repo root
_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from nose_eo_track_helpers import (  # noqa: E402
    AREA_JUMP_MAX,
    AREA_JUMP_MIN,
    ROI_EXPAND_BASE,
    ROI_EXPAND_MAX,
    area_growth_ratio,
    area_jump_ok,
    est_jump_ok,
    est_jump_ratio,
    iou_xywh,
    motion_vector_coherence,
    reacquire_roi_norm,
    roi_covers_point,
    roi_expand_for_closing,
    select_reacquire_candidate,
    trajectory_evidence,
)
from nose_eo_demo_server import Box, pick_target_box  # noqa: E402
from nose_eo_dataset import save_negative_sample  # noqa: E402


def _box(x: float, y: float, w: float, h: float, conf: float = 0.8) -> dict:
    return {"x": x, "y": y, "w": w, "h": h, "conf": conf, "cls": "track"}


def main() -> int:
    passed = 0
    failed = 0

    def check(name: str, cond: bool) -> None:
        nonlocal passed, failed
        if cond:
            passed += 1
            print(f"  PASS  {name}")
        else:
            failed += 1
            print(f"  FAIL  {name}")

    print("=== Nose EO track helpers (host, no camera/YOLO) ===")

    # --- IoU ---
    a = _box(0.4, 0.4, 0.2, 0.2)
    b = _box(0.4, 0.4, 0.2, 0.2)
    check("iou identical boxes == 1", abs(iou_xywh(a, b) - 1.0) < 1e-6)

    c = _box(0.7, 0.7, 0.2, 0.2)
    check("iou disjoint boxes == 0", iou_xywh(a, c) == 0.0)

    d = _box(0.5, 0.4, 0.2, 0.2)  # half overlap in x
    iou_half = iou_xywh(a, d)
    check("iou partial overlap in (0,1)", 0.0 < iou_half < 1.0)

    # --- Adaptive ROI expand ---
    e0 = roi_expand_for_closing(ROI_EXPAND_BASE, 1.0, False)
    check("roi_expand steady == base 4.0", abs(e0 - ROI_EXPAND_BASE) < 1e-9)

    e_close = roi_expand_for_closing(ROI_EXPAND_BASE, 1.0, True)
    check("roi_expand closing >= 5.0", e_close >= 5.0 - 1e-9)

    e_grow = roi_expand_for_closing(ROI_EXPAND_BASE, 1.5, False)
    check("roi_expand growth>1.3 >= 5.0", e_grow >= 5.0 - 1e-9)

    e_max = roi_expand_for_closing(ROI_EXPAND_BASE, 2.5, True)
    check("roi_expand high growth <= max 8.0", e_max <= ROI_EXPAND_MAX + 1e-9)

    # --- Area jump reject ---
    last = _box(0.4, 0.4, 0.10, 0.10)  # area 0.01
    ok_cand = _box(0.41, 0.41, 0.12, 0.12)  # ~1.44x
    spike = _box(0.4, 0.4, 0.25, 0.25)  # 6.25x
    collapse = _box(0.4, 0.4, 0.03, 0.03)  # 0.09x
    check("area_jump accept ~1.4x", area_jump_ok(ok_cand, last))
    check("area_jump reject >2x spike", not area_jump_ok(spike, last))
    check("area_jump reject <0.4 collapse", not area_jump_ok(collapse, last))

    # Growth ratio helper
    growing = _box(0.4, 0.4, 0.15, 0.15)
    g = area_growth_ratio(growing, last)
    check("area_growth_ratio > 1 when growing", g > 1.0)

    # --- select_reacquire_candidate rejects spike ---
    picked = select_reacquire_candidate([spike, ok_cand], last)
    check("select prefers non-spike candidate", picked is ok_cand)
    rejected = select_reacquire_candidate([spike], last)
    check("select rejects sole >2x candidate", rejected is None)

    # After an occlusion, prefer a nearby same-size candidate even when a
    # farther distractor has slightly higher confidence.
    near = _box(0.53, 0.40, 0.10, 0.10, conf=0.55)
    far = _box(0.78, 0.40, 0.10, 0.10, conf=0.95)
    check(
        "reacquire prefers trajectory proximity over far distractor",
        select_reacquire_candidate([far, near], last, iou_min=0.03) is near,
    )

    # --- Synthetic fast translate + growing bbox ---
    # Simulate last boxes growing while center translates right/down.
    seq = []
    cx, cy, side = 0.30, 0.40, 0.06
    for i in range(6):
        side *= 1.18  # grow each step
        cx += 0.04
        cy += 0.015
        seq.append(_box(cx - side / 2, cy - side / 2, side, side))

    # Adaptive expand from last→current growth should enlarge ROI
    prev, curr = seq[-2], seq[-1]
    growth = area_growth_ratio(curr, prev)
    expand = roi_expand_for_closing(ROI_EXPAND_BASE, growth, closing=True)
    roi = reacquire_roi_norm(curr, expand)
    # Expected next center continues translate
    expect_cx = cx + 0.04
    expect_cy = cy + 0.015
    check(
        "reacquire ROI covers expected translate center",
        roi_covers_point(roi, expect_cx, expect_cy),
    )
    check("synthetic growth triggers expand > base", expand > ROI_EXPAND_BASE)

    # >2x area candidate relative to last in sequence must be rejected
    last_seq = seq[-1]
    huge = _box(last_seq["x"], last_seq["y"], last_seq["w"] * 2.2, last_seq["h"] * 2.2)
    check(
        "synthetic >2x area candidate rejected",
        not area_jump_ok(huge, last_seq)
        and select_reacquire_candidate([huge], last_seq) is None,
    )

    # Constants sanity (knob docs)
    check("AREA_JUMP_MAX == 2.0", AREA_JUMP_MAX == 2.0)
    check("AREA_JUMP_MIN == 0.4", AREA_JUMP_MIN == 0.4)

    check("est_jump accept 1.5x", est_jump_ok(100.0, 150.0, max_ratio=2.0))
    check("est_jump reject 2.5x", not est_jump_ok(100.0, 250.0, max_ratio=2.0))
    check("est_jump_ratio 1.8", abs(est_jump_ratio(100.0, 180.0) - 1.8) < 1e-9)

    # --- Motion qualification: translation vs anchored oscillation ---
    translating = [(0.10 + i * 0.02, 0.40 + i * 0.004) for i in range(10)]
    translation_ev = trajectory_evidence(translating)
    check("translation has high linearity", float(translation_ev["linearity"]) > 0.95)
    check("translation has forward progress", float(translation_ev["forward_ratio"]) > 0.95)
    check("translation has no reversals", int(translation_ev["reversals"]) == 0)

    oscillating = [(0.50 + (0.012 if i % 2 else -0.012), 0.50) for i in range(10)]
    oscillation_ev = trajectory_evidence(oscillating)
    check("anchored oscillation has low net progress", float(oscillation_ev["linearity"]) < 0.2)
    check("anchored oscillation has reversals", int(oscillation_ev["reversals"]) >= 3)

    rigid_vectors = [(4.0, 1.0), (4.2, 0.9), (3.8, 1.1), (4.1, 1.0)]
    rigid_speed, rigid_coherence = motion_vector_coherence(rigid_vectors)
    check("rigid flow has usable speed", rigid_speed > 3.0)
    check("rigid flow is coherent", rigid_coherence >= 0.9)

    deforming_vectors = [(3.0, 0.0), (-3.0, 0.0), (0.0, 3.0), (0.0, -3.0)]
    deform_speed, deform_coherence = motion_vector_coherence(deforming_vectors)
    check("deforming flow is rejected", deform_speed == 0.0 or deform_coherence < 0.5)

    check(
        "bird cannot seed automatic demo track",
        pick_target_box([Box(0.2, 0.2, 0.1, 0.1, "bird", 0.95)]) is None,
    )
    check(
        "person cannot seed automatic demo track",
        pick_target_box([Box(0.2, 0.2, 0.1, 0.2, "person", 0.95)]) is None,
    )
    check(
        "airplane remains an eligible semantic demo candidate",
        pick_target_box([Box(0.2, 0.2, 0.1, 0.1, "airplane", 0.75)]) is not None,
    )

    # --- Explicit hard-negative capture: atomic save + hash deduplication ---
    with tempfile.TemporaryDirectory(prefix="nose-eo-negative-test-") as tmp:
        out_dir = Path(tmp)
        fake_jpeg = b"\xff\xd8unit-test-jpeg\xff\xd9"
        first = save_negative_sample(
            fake_jpeg,
            out_dir,
            reason="tree / curtain",
            session_id="outdoor-tree-1",
            frame_index=0,
        )
        second = save_negative_sample(fake_jpeg, out_dir, reason="tree / curtain")
        check("negative capture writes one JPEG", first["saved"] and first["count"] == 1)
        check("negative capture deduplicates identical frame", second["duplicate"] and second["count"] == 1)
        metadata_lines = (out_dir / "capture_meta.jsonl").read_text(encoding="utf-8").splitlines()
        check("negative capture writes one metadata row", len(metadata_lines) == 1)
        metadata = json.loads(metadata_lines[0])
        check("negative capture stamps session id", metadata["session_id"] == "outdoor-tree-1")
        check("negative capture stamps frame index", metadata["frame_index"] == 0)

    print(f"\nResult: {passed} PASS, {failed} FAIL")
    if failed:
        print("OVERALL FAIL")
        return 1
    print("OVERALL PASS")
    return 0



def test_est_jump_ok_within_2x():
    assert est_jump_ok(100.0, 150.0, max_ratio=2.0)
    assert not est_jump_ok(100.0, 250.0, max_ratio=2.0)
    assert abs(est_jump_ratio(100.0, 180.0) - 1.8) < 1e-9


if __name__ == "__main__":
    raise SystemExit(main())
