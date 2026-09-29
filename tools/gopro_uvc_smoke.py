#!/usr/bin/env python3
"""Smoke-test GoPro Webcam Mode / USB UVC cameras with OpenCV.

Lists video devices, opens one (preferring GoPro/Webcam/USB in the name when
available), grabs a single frame, and optionally writes it to disk.

Mac setup (OpenCV not required on the Linux box):
  brew install opencv
  # or: pip install opencv-python

Usage:
  python tools/gopro_uvc_smoke.py
  python tools/gopro_uvc_smoke.py --index 0 --out /tmp/gopro_frame.jpg
  python tools/gopro_uvc_smoke.py --list-only
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

PREFERRED_RE = re.compile(r"GoPro|Webcam|USB", re.I)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=int, default=None, help="Force camera index")
    parser.add_argument("--out", type=Path, default=None, help="Optional JPEG path for one frame")
    parser.add_argument("--list-only", action="store_true", help="Only probe/list indices 0..7")
    parser.add_argument("--max-probe", type=int, default=8, help="Highest index to probe")
    args = parser.parse_args()

    try:
        import cv2  # type: ignore
    except ImportError:
        print(
            "OpenCV (cv2) is not installed.\n"
            "  Mac: brew install opencv   OR   pip install opencv-python\n"
            "  Linux: pip install opencv-python\n"
            "This script is optional; the GCS UI uses browser getUserMedia instead.",
            file=sys.stderr,
        )
        return 2

    print(f"OpenCV {cv2.__version__}")

    found: list[tuple[int, str]] = []
    for i in range(args.max_probe):
        cap = cv2.VideoCapture(i)
        if not cap.isOpened():
            cap.release()
            continue
        # Backend name / backend-specific props vary; label what we can.
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH) or 0)
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT) or 0)
        label = f"index={i} {w}x{h}"
        found.append((i, label))
        print(f"  open OK  {label}")
        cap.release()

    if not found:
        print("No cameras opened. Put GoPro in Webcam Mode and reconnect USB.", file=sys.stderr)
        return 1

    if args.list_only:
        return 0

    if args.index is not None:
        pick = args.index
    else:
        # Prefer indices whose probe string matches; else first openable.
        preferred = [i for i, lab in found if PREFERRED_RE.search(lab)]
        pick = preferred[0] if preferred else found[0][0]

    print(f"Opening camera index {pick} …")
    cap = cv2.VideoCapture(pick)
    if not cap.isOpened():
        print(f"Failed to open index {pick}", file=sys.stderr)
        return 1

    ok, frame = cap.read()
    cap.release()
    if not ok or frame is None:
        print("Opened but failed to grab a frame.", file=sys.stderr)
        return 1

    h, w = frame.shape[:2]
    print(f"Grabbed frame {w}x{h}")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        if not cv2.imwrite(str(args.out), frame):
            print(f"Failed to write {args.out}", file=sys.stderr)
            return 1
        print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
