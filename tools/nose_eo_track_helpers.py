"""Pure helpers for Nose EO DEMO track re-acquire (no camera / YOLO required).

Used by tools/nose_eo_demo_server.py and tools/test_nose_eo_tracking.py.
DEMO only — never class Lock.
"""
from __future__ import annotations

import math
import statistics
from typing import Any, Mapping, Sequence

# ---------------------------------------------------------------------------
# Knobs (documented in tools/README_NOSE_EO_DEMO.md)
# ---------------------------------------------------------------------------
ROI_EXPAND_BASE = 4.0
ROI_EXPAND_MAX = 8.0
ROI_EXPAND_GROWTH_TRIGGER = 1.3  # area growth ratio that starts padding up
AREA_JUMP_MAX = 2.0  # reject cand if cand.area / last.area > this
AREA_JUMP_MIN = 0.4  # reject cand if cand.area / last.area < this
IOU_ACCEPT_MIN = 0.12


def trajectory_evidence(points: Sequence[tuple[float, float]]) -> dict[str, float | int]:
    """Measure whether a center trail translates instead of oscillating.

    Leaves, curtains and reflections often accumulate plenty of local motion,
    but repeatedly reverse around the same anchor. A translating object should
    make net progress in one dominant direction with little backtracking.
    Coordinates may be pixels or normalized image coordinates.
    """
    if len(points) < 2:
        return {
            "net": 0.0,
            "path": 0.0,
            "linearity": 0.0,
            "forward_ratio": 0.0,
            "reversals": 0,
        }

    segments: list[tuple[float, float]] = []
    path = 0.0
    for previous, current in zip(points, points[1:]):
        dx = float(current[0]) - float(previous[0])
        dy = float(current[1]) - float(previous[1])
        segments.append((dx, dy))
        path += math.hypot(dx, dy)

    net_x = float(points[-1][0]) - float(points[0][0])
    net_y = float(points[-1][1]) - float(points[0][1])
    net = math.hypot(net_x, net_y)
    if path <= 1e-12 or net <= 1e-12:
        return {
            "net": net,
            "path": path,
            "linearity": 0.0,
            "forward_ratio": 0.0,
            "reversals": sum(1 for dx, dy in segments if math.hypot(dx, dy) > 1e-9),
        }

    ux, uy = net_x / net, net_y / net
    projections = [dx * ux + dy * uy for dx, dy in segments]
    forward = sum(max(0.0, value) for value in projections)
    backward = sum(max(0.0, -value) for value in projections)
    meaningful = max(path * 0.04, 1e-6)
    reversals = sum(1 for value in projections if value < -meaningful)
    return {
        "net": net,
        "path": path,
        "linearity": net / path,
        "forward_ratio": forward / max(forward + backward, 1e-12),
        "reversals": reversals,
    }


def motion_vector_coherence(
    vectors: Sequence[tuple[float, float]],
    *,
    min_vector_magnitude: float = 0.25,
) -> tuple[float, float]:
    """Return robust median motion magnitude and directional coherence.

    Rigid translation produces feature vectors with a shared direction. Foliage
    and fabric deformation produces a wide spread of directions even when the
    aggregate foreground mask is large.
    """
    usable = [
        (float(dx), float(dy))
        for dx, dy in vectors
        if math.isfinite(dx)
        and math.isfinite(dy)
        and math.hypot(dx, dy) >= min_vector_magnitude
    ]
    if not usable:
        return 0.0, 0.0

    median_dx = statistics.median(dx for dx, _ in usable)
    median_dy = statistics.median(dy for _, dy in usable)
    median_magnitude = math.hypot(median_dx, median_dy)
    if median_magnitude <= 1e-9:
        return 0.0, 0.0

    ux, uy = median_dx / median_magnitude, median_dy / median_magnitude
    coherent = 0
    for dx, dy in usable:
        magnitude = math.hypot(dx, dy)
        cosine = (dx * ux + dy * uy) / magnitude
        magnitude_ratio = magnitude / median_magnitude
        if cosine >= 0.65 and 0.25 <= magnitude_ratio <= 4.0:
            coherent += 1
    return median_magnitude, coherent / len(usable)


def box_area(box: Mapping[str, Any] | Any) -> float:
    """Normalized box area (w*h). Accepts dict or object with .w/.h."""
    w = float(_attr(box, "w"))
    h = float(_attr(box, "h"))
    return max(0.0, w * h)


def area_growth_ratio(curr: Mapping[str, Any] | Any, prev: Mapping[str, Any] | Any | None) -> float:
    """curr.area / prev.area; 1.0 if prev missing or zero area."""
    if prev is None:
        return 1.0
    pa = box_area(prev)
    if pa <= 1e-12:
        return 1.0
    return box_area(curr) / pa


def roi_expand_for_closing(
    base_expand: float,
    area_growth_ratio: float,
    closing: bool,
    *,
    max_expand: float = ROI_EXPAND_MAX,
    growth_trigger: float = ROI_EXPAND_GROWTH_TRIGGER,
) -> float:
    """Grow ROI pad when box area is increasing or closing flag is true.

    Examples (base=4.0, max=8.0):
      growth=1.0, closing=False → 4.0
      growth>1.3 or closing     → up to 8.0
    """
    base = float(base_expand)
    mx = float(max_expand)
    if mx < base:
        mx = base
    g = float(area_growth_ratio)
    need = bool(closing) or g > float(growth_trigger)
    if not need:
        # Mild grow when area increasing but below trigger
        if g > 1.0:
            t = min(1.0, (g - 1.0) / max(growth_trigger - 1.0, 1e-6))
            return base + t * (min(3.5, mx) - base) * 0.5
        return base
    # Map growth [trigger .. ~2.0] → [mid .. max]; closing alone → at least 3.5
    if g > growth_trigger:
        t = min(1.0, (g - growth_trigger) / max(2.0 - growth_trigger, 1e-6))
    else:
        t = 0.0
    target_floor = max(base, 5.0) if closing or g > growth_trigger else base
    expanded = base + t * (mx - base)
    return float(min(mx, max(base, target_floor, expanded)))


def area_jump_ok(
    cand: Mapping[str, Any] | Any,
    last: Mapping[str, Any] | Any,
    *,
    max_ratio: float = AREA_JUMP_MAX,
    min_ratio: float = AREA_JUMP_MIN,
) -> bool:
    """True if candidate area vs last is within [min_ratio, max_ratio]."""
    la = box_area(last)
    if la <= 1e-12:
        return True
    r = box_area(cand) / la
    return min_ratio <= r <= max_ratio


def iou_xywh(a: Mapping[str, Any] | Any, b: Mapping[str, Any] | Any) -> float:
    """IoU of two axis-aligned boxes in x,y,w,h (normalized or pixels)."""
    ax, ay, aw, ah = float(_attr(a, "x")), float(_attr(a, "y")), float(_attr(a, "w")), float(_attr(a, "h"))
    bx, by, bw, bh = float(_attr(b, "x")), float(_attr(b, "y")), float(_attr(b, "w")), float(_attr(b, "h"))
    ax2, ay2 = ax + aw, ay + ah
    bx2, by2 = bx + bw, by + bh
    ix0, iy0 = max(ax, bx), max(ay, by)
    ix1, iy1 = min(ax2, bx2), min(ay2, by2)
    inter = max(0.0, ix1 - ix0) * max(0.0, iy1 - iy0)
    union = aw * ah + bw * bh - inter + 1e-9
    return inter / union


def reacquire_roi_norm(
    last_box: Mapping[str, Any] | Any,
    roi_expand: float,
) -> tuple[float, float, float, float]:
    """Expanded ROI around last box as (x0, y0, x1, y1) in normalized coords."""
    cx = float(_attr(last_box, "x")) + float(_attr(last_box, "w")) / 2.0
    cy = float(_attr(last_box, "y")) + float(_attr(last_box, "h")) / 2.0
    rw = min(1.0, float(_attr(last_box, "w")) * float(roi_expand))
    rh = min(1.0, float(_attr(last_box, "h")) * float(roi_expand))
    x0 = max(0.0, cx - rw / 2.0)
    y0 = max(0.0, cy - rh / 2.0)
    x1 = min(1.0, x0 + rw)
    y1 = min(1.0, y0 + rh)
    # Re-clamp width if clipped
    if x1 - x0 < rw and cx + rw / 2.0 > 1.0:
        x0 = max(0.0, x1 - rw)
    if y1 - y0 < rh and cy + rh / 2.0 > 1.0:
        y0 = max(0.0, y1 - rh)
    return x0, y0, x1, y1


def roi_covers_point(
    roi: Sequence[float],
    px: float,
    py: float,
) -> bool:
    """True if normalized point (px, py) lies inside ROI (x0,y0,x1,y1)."""
    x0, y0, x1, y1 = (float(v) for v in roi)
    return x0 <= px <= x1 and y0 <= py <= y1


def select_reacquire_candidate(
    candidates: Sequence[Mapping[str, Any] | Any],
    last_box: Mapping[str, Any] | Any,
    *,
    conf_weight: float = 0.15,
    iou_min: float = IOU_ACCEPT_MIN,
    max_area_ratio: float = AREA_JUMP_MAX,
    min_area_ratio: float = AREA_JUMP_MIN,
) -> Any | None:
    """Pick best candidate by overlap, center proximity, and confidence.

    Candidates are box-like (x,y,w,h[,conf]). Returns the winning object or None.
    """
    best = None
    best_score = -1.0
    for cand in candidates:
        if not area_jump_ok(cand, last_box, max_ratio=max_area_ratio, min_ratio=min_area_ratio):
            continue
        iou = iou_xywh(last_box, cand)
        lcx = float(_attr(last_box, "x")) + float(_attr(last_box, "w")) / 2.0
        lcy = float(_attr(last_box, "y")) + float(_attr(last_box, "h")) / 2.0
        ccx = float(_attr(cand, "x")) + float(_attr(cand, "w")) / 2.0
        ccy = float(_attr(cand, "y")) + float(_attr(cand, "h")) / 2.0
        center_dist = ((lcx - ccx) ** 2 + (lcy - ccy) ** 2) ** 0.5
        proximity = max(0.0, 1.0 - center_dist / 0.5)
        cf = float(_attr(cand, "conf", default=0.0))
        score = iou + 0.35 * proximity + conf_weight * cf
        if score > best_score:
            best_score = score
            best = cand
    if best is not None and best_score >= iou_min:
        return best
    return None


def _attr(obj: Mapping[str, Any] | Any, key: str, default: float | None = None) -> Any:
    if isinstance(obj, Mapping):
        if key in obj:
            return obj[key]
        if default is not None:
            return default
        raise KeyError(key)
    if hasattr(obj, key):
        return getattr(obj, key)
    if default is not None:
        return default
    raise AttributeError(key)


def est_jump_ok(prev_range_m: float | None, cur_range_m: float | None, max_ratio: float = 2.0) -> bool:
    """True if EST did not jump by more than max_ratio in one step (lab card)."""
    if prev_range_m is None or cur_range_m is None:
        return True
    if prev_range_m <= 1e-6 or cur_range_m <= 1e-6:
        return False
    ratio = max(prev_range_m, cur_range_m) / min(prev_range_m, cur_range_m)
    return ratio <= max_ratio


def est_jump_ratio(prev_range_m: float, cur_range_m: float) -> float:
    if prev_range_m <= 1e-6:
        return float("inf")
    return float(cur_range_m) / float(prev_range_m)
