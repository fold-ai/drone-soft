#!/usr/bin/env python3
"""Measure camera FPS/read latency and exercise reconnect behavior.

This is a passive bench tool. It opens only the selected camera and writes a
JSON report; it has no flight-controller or network integration.
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
from datetime import datetime, timezone
from pathlib import Path


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * fraction)))
    return ordered[index]


def parse_device(raw: str) -> int | str:
    return int(raw) if raw.isdigit() else raw


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="0", help="OpenCV index or device path")
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--expected-fps", type=float, default=30.0)
    parser.add_argument("--min-fps", type=float, default=20.0)
    parser.add_argument("--max-read-p95-ms", type=float, default=80.0)
    parser.add_argument("--reopen-after-failures", type=int, default=3)
    parser.add_argument("--max-reconnects", type=int, default=3)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--fps", type=float, default=30.0)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.duration <= 0 or args.expected_fps <= 0 or args.reopen_after_failures < 1:
        parser.error("duration/FPS must be positive and reopen threshold must be >= 1")

    try:
        import cv2
    except ImportError as exc:
        print(f"camera bench requires OpenCV: {exc}")
        return 2

    device = parse_device(args.device)

    def open_camera():
        capture = cv2.VideoCapture(device)
        capture.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        capture.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        capture.set(cv2.CAP_PROP_FPS, args.fps)
        return capture

    capture = open_camera()
    start = time.monotonic()
    deadline = start + args.duration
    frame_times: list[float] = []
    read_ms: list[float] = []
    failures = 0
    consecutive_failures = 0
    reconnects = 0
    first_shape: tuple[int, int] | None = None

    try:
        while time.monotonic() < deadline:
            read_start = time.monotonic()
            ok, frame = capture.read()
            read_end = time.monotonic()
            read_ms.append((read_end - read_start) * 1000.0)
            if not ok or frame is None:
                failures += 1
                consecutive_failures += 1
                if consecutive_failures >= args.reopen_after_failures:
                    capture.release()
                    reconnects += 1
                    if reconnects > args.max_reconnects:
                        break
                    time.sleep(min(2.0, 0.25 * (2 ** (reconnects - 1))))
                    capture = open_camera()
                    consecutive_failures = 0
                else:
                    time.sleep(0.02)
                continue
            consecutive_failures = 0
            frame_times.append(read_end)
            if first_shape is None:
                first_shape = (int(frame.shape[1]), int(frame.shape[0]))
    finally:
        capture.release()

    elapsed = max(time.monotonic() - start, 1e-9)
    fps = len(frame_times) / elapsed
    gaps = [b - a for a, b in zip(frame_times, frame_times[1:])]
    drop_threshold = 1.5 / args.expected_fps
    estimated_drops = sum(max(0, round(gap * args.expected_fps) - 1) for gap in gaps if gap > drop_threshold)
    p95 = percentile(read_ms, 0.95)
    blockers: list[str] = []
    if not frame_times:
        blockers.append("no frames captured")
    if fps < args.min_fps:
        blockers.append(f"measured FPS {fps:.2f} < minimum {args.min_fps:.2f}")
    if p95 is not None and p95 > args.max_read_p95_ms:
        blockers.append(f"read p95 {p95:.2f}ms > maximum {args.max_read_p95_ms:.2f}ms")
    if reconnects > args.max_reconnects:
        blockers.append(f"reconnects {reconnects} > maximum {args.max_reconnects}")

    payload = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "scope": "passive_camera_reconnect_bench",
        "status": "PASS" if not blockers else "FAIL",
        "blockers": blockers,
        "device": args.device,
        "requested": {"width": args.width, "height": args.height, "fps": args.fps},
        "observed_resolution": (
            {"width": first_shape[0], "height": first_shape[1]} if first_shape else None
        ),
        "duration_s": elapsed,
        "frames": len(frame_times),
        "fps": fps,
        "read_latency_ms": {
            "median": statistics.median(read_ms) if read_ms else None,
            "p95": p95,
            "max": max(read_ms) if read_ms else None,
        },
        "read_failures": failures,
        "reconnects": reconnects,
        "estimated_dropped_frames": estimated_drops,
        "flight_control_access": False,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2))
    print(f"wrote {args.out}")
    return 0 if not blockers else 1


if __name__ == "__main__":
    raise SystemExit(main())
