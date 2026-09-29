#!/usr/bin/env python3
"""Latency probe — camera→Orin and E2E capture→track publish gates.

Budgets (V1 ICD docs/icd/latency_budget.md):
  camera → Orin (dequeue / ready):  ≤ 30–40 ms
  E2E capture → track publish:      ≤ 50–80 ms
  frame → box (perception):         p95 < 40 ms

Reads JSONL with measurement_epoch_ns / publish_time_ns (or float seconds).
No heavy deps — stdlib only (optional numpy for p95 if present).
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import List, Optional, Tuple


def _percentile(sorted_vals: List[float], p: float) -> float:
    if not sorted_vals:
        return float("nan")
    try:
        import numpy as np  # optional

        return float(np.percentile(sorted_vals, p))
    except ImportError:
        k = (len(sorted_vals) - 1) * (p / 100.0)
        f = math.floor(k)
        c = math.ceil(k)
        if f == c:
            return sorted_vals[int(k)]
        return sorted_vals[f] * (c - k) + sorted_vals[c] * (k - f)


def _ms(a_ns: int, b_ns: int) -> float:
    return (b_ns - a_ns) / 1e6


def load_pairs(path: Path, epoch_key: str, publish_key: str) -> List[Tuple[int, int]]:
    pairs: List[Tuple[int, int]] = []
    with path.open() as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            row = json.loads(line)
            # Accept ns ints or float seconds (t_pps style)
            if epoch_key in row and publish_key in row:
                e, p = row[epoch_key], row[publish_key]
            elif "t_pps" in row and "t_publish_pps" in row:
                e = int(float(row["t_pps"]) * 1e9)
                p = int(float(row["t_publish_pps"]) * 1e9)
            elif "measurement_epoch" in row and "publish_time_ns" in row:
                e, p = row["measurement_epoch"], row["publish_time_ns"]
                if isinstance(e, float) and e < 1e12:
                    e = int(e * 1e9)
            else:
                continue
            if isinstance(e, float) and e < 1e12:
                e = int(e * 1e9)
            if isinstance(p, float) and p < 1e12:
                p = int(p * 1e9)
            pairs.append((int(e), int(p)))
    return pairs


def evaluate(name: str, lat_ms: List[float], lo: float, hi: float) -> bool:
    if not lat_ms:
        print(f"FAIL  {name}: no samples")
        return False
    lat_ms = sorted(lat_ms)
    p50 = _percentile(lat_ms, 50)
    p95 = _percentile(lat_ms, 95)
    mx = lat_ms[-1]
    # Pass if p95 within upper budget (hi); warn if above lo mid-band
    ok = p95 <= hi
    status = "PASS" if ok else "FAIL"
    print(
        f"{status} {name}: n={len(lat_ms)} p50={p50:.2f} ms p95={p95:.2f} ms "
        f"max={mx:.2f} ms  budget=[{lo:.0f},{hi:.0f}] ms"
    )
    return ok


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--camera-jsonl",
        type=Path,
        help="camera_ts.jsonl: measurement_epoch_ns → publish_time_ns (cam→Orin)",
    )
    ap.add_argument(
        "--track-jsonl",
        type=Path,
        help="tracks.jsonl: measurement_epoch_ns → publish_time_ns (E2E)",
    )
    ap.add_argument("--cam-lo", type=float, default=30.0)
    ap.add_argument("--cam-hi", type=float, default=40.0)
    ap.add_argument("--e2e-lo", type=float, default=50.0)
    ap.add_argument("--e2e-hi", type=float, default=80.0)
    ap.add_argument(
        "--demo",
        action="store_true",
        help="Run synthetic samples to exercise pass/fail reporting",
    )
    args = ap.parse_args(argv)

    ok_all = True
    if args.demo:
        cam = [12.0, 18.0, 22.0, 28.0, 35.0]
        e2e = [40.0, 45.0, 52.0, 60.0, 70.0]
        ok_all &= evaluate("camera→Orin (demo)", cam, args.cam_lo, args.cam_hi)
        ok_all &= evaluate("E2E capture→track (demo)", e2e, args.e2e_lo, args.e2e_hi)
        return 0 if ok_all else 2

    if not args.camera_jsonl and not args.track_jsonl:
        ap.error("provide --camera-jsonl and/or --track-jsonl (or --demo)")

    if args.camera_jsonl:
        pairs = load_pairs(
            args.camera_jsonl, "measurement_epoch_ns", "publish_time_ns"
        )
        lat = [_ms(e, p) for e, p in pairs]
        ok_all &= evaluate("camera→Orin", lat, args.cam_lo, args.cam_hi)

    if args.track_jsonl:
        pairs = load_pairs(
            args.track_jsonl, "measurement_epoch_ns", "publish_time_ns"
        )
        lat = [_ms(e, p) for e, p in pairs]
        ok_all &= evaluate("E2E capture→track publish", lat, args.e2e_lo, args.e2e_hi)

    return 0 if ok_all else 2


if __name__ == "__main__":
    sys.exit(main())
