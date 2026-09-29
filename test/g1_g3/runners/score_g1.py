#!/usr/bin/env python3
"""Score a G1 archive folder. Prints PASS/FAIL recommendation; does not write markers."""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

HZ_MIN = 60.0
LAT_P95_MAX_MS = 40.0


def p95(xs: list[float]) -> float:
    if not xs:
        return math.inf
    ys = sorted(xs)
    i = max(0, min(len(ys) - 1, math.ceil(0.95 * len(ys)) - 1))
    return ys[i]


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    out = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def finite_number(value: object) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path)
    args = p.parse_args()
    root = args.archive
    reasons: list[str] = []

    dets = load_jsonl(root / "streams" / "detections.jsonl")
    hz_rows = load_jsonl(root / "streams" / "detector_hz.jsonl")
    sync = load_jsonl(root / "streams" / "sync_status.jsonl")
    imu = load_jsonl(root / "streams" / "imu.jsonl")

    lat = [n for d in dets if (n := finite_number(d.get("frame_to_box_ms"))) is not None]
    if len(lat) != len(dets):
        reasons.append("missing or non-finite frame_to_box_ms")
    lat_p95 = p95(lat)
    if lat_p95 >= LAT_P95_MAX_MS:
        reasons.append(f"frame→box p95 {lat_p95:.2f} ms >= {LAT_P95_MAX_MS}")

    hz_vals = [n for r in hz_rows if (n := finite_number(r.get("hz"))) is not None]
    if len(hz_vals) != len(hz_rows) or not hz_rows:
        reasons.append("detector Hz windows missing or non-finite")
    hz_sustained = min(hz_vals) if hz_vals else 0.0
    if hz_sustained < HZ_MIN:
        reasons.append(f"detector Hz sustained {hz_sustained:.2f} < {HZ_MIN}")

    frame_epochs = [finite_number(d.get("t_pps")) for d in dets]
    pps_frames = bool(dets) and all(t is not None and t > 0 for t in frame_epochs)
    if not pps_frames:
        reasons.append("PPS mid-exposure missing on detections")

    pps_imu = bool(imu) and all(
        (t := finite_number(r.get("imu_ts"))) is not None
        and t > 0
        and r.get("sync_ok") is True
        for r in imu
    )
    pps_imu = pps_imu and bool(sync) and all(s.get("pps_locked") is True for s in sync)
    if not pps_imu:
        reasons.append("PPS/DRDY stamps missing or sync_ok false on IMU")

    result = "PASS" if not reasons else "FAIL"
    summary = {
        "gate": "G1",
        "result": result,
        "detector_hz_sustained": hz_sustained,
        "frame_to_box_ms_p95": None if math.isinf(lat_p95) else lat_p95,
        "pps_frames_ok": pps_frames,
        "pps_imu_ok": pps_imu,
        "windows_scored": max(1, len(hz_vals)),
        "fail_reasons": reasons,
        "archive_path": str(root),
    }
    print(json.dumps(summary, indent=2))
    return 0 if result == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
