#!/usr/bin/env python3
"""Offline checkerboard calibration for an ELP camera image set.

The tool reads recorded images and writes a standalone JSON artifact. It never
opens flight-control devices and does not modify the flight calibration YAML.
An operator must review the result before promoting it into a profile.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}


def image_paths(folder: Path) -> list[Path]:
    return sorted(
        path for path in folder.iterdir() if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", type=Path, help="folder containing checkerboard images")
    parser.add_argument("--cols", type=int, default=9, help="inner corners per row")
    parser.add_argument("--rows", type=int, default=6, help="inner corners per column")
    parser.add_argument("--square-mm", type=float, required=True)
    parser.add_argument("--min-views", type=int, default=12)
    parser.add_argument("--max-rms-px", type=float, default=1.0)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    if args.cols < 3 or args.rows < 3 or args.square_mm <= 0 or args.min_views < 3:
        parser.error("invalid checkerboard geometry")
    if not args.images.is_dir():
        parser.error(f"image folder does not exist: {args.images}")

    try:
        import cv2
        import numpy as np
    except ImportError as exc:
        print(f"calibration requires OpenCV and NumPy: {exc}")
        return 2

    pattern = (args.cols, args.rows)
    object_template = np.zeros((args.cols * args.rows, 3), np.float32)
    object_template[:, :2] = np.mgrid[0 : args.cols, 0 : args.rows].T.reshape(-1, 2)
    object_template *= float(args.square_mm) / 1000.0

    object_points: list[object] = []
    image_points: list[object] = []
    accepted: list[str] = []
    rejected: list[dict[str, str]] = []
    seen_hashes: set[str] = set()
    image_size: tuple[int, int] | None = None

    for path in image_paths(args.images):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest in seen_hashes:
            rejected.append({"image": path.name, "reason": "duplicate"})
            continue
        seen_hashes.add(digest)
        frame = cv2.imread(str(path), cv2.IMREAD_COLOR)
        if frame is None:
            rejected.append({"image": path.name, "reason": "decode_failed"})
            continue
        height, width = frame.shape[:2]
        if image_size is None:
            image_size = (width, height)
        elif image_size != (width, height):
            rejected.append({"image": path.name, "reason": "resolution_mismatch"})
            continue

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        found = False
        corners = None
        if hasattr(cv2, "findChessboardCornersSB"):
            found, corners = cv2.findChessboardCornersSB(
                gray,
                pattern,
                flags=cv2.CALIB_CB_EXHAUSTIVE | cv2.CALIB_CB_ACCURACY,
            )
        if not found:
            found, corners = cv2.findChessboardCorners(
                gray,
                pattern,
                flags=cv2.CALIB_CB_ADAPTIVE_THRESH | cv2.CALIB_CB_NORMALIZE_IMAGE,
            )
            if found:
                corners = cv2.cornerSubPix(
                    gray,
                    corners,
                    (11, 11),
                    (-1, -1),
                    (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_MAX_ITER, 40, 1e-4),
                )
        if not found or corners is None:
            rejected.append({"image": path.name, "reason": "checkerboard_not_found"})
            continue
        object_points.append(object_template.copy())
        image_points.append(corners)
        accepted.append(path.name)

    blockers: list[str] = []
    if len(accepted) < args.min_views:
        blockers.append(f"accepted views {len(accepted)} < required {args.min_views}")
    if image_size is None:
        blockers.append("no decodable images")

    payload: dict[str, object] = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "scope": "offline_camera_intrinsics",
        "images": str(args.images.resolve()),
        "checkerboard": {
            "inner_cols": args.cols,
            "inner_rows": args.rows,
            "square_m": args.square_mm / 1000.0,
        },
        "accepted_images": accepted,
        "rejected_images": rejected,
        "status": "BLOCKED" if blockers else "PENDING_CALIBRATION",
        "blockers": blockers,
        "promoted_to_flight_profile": False,
    }

    if not blockers:
        rms, matrix, distortion, rotations, translations = cv2.calibrateCamera(
            object_points, image_points, image_size, None, None
        )
        per_view_rmse: list[float] = []
        for obj, observed, rotation, translation in zip(
            object_points, image_points, rotations, translations
        ):
            projected, _ = cv2.projectPoints(obj, rotation, translation, matrix, distortion)
            error = cv2.norm(observed, projected, cv2.NORM_L2)
            per_view_rmse.append(float(error / math.sqrt(len(projected))))
        blockers = [] if float(rms) <= args.max_rms_px else [
            f"calibration RMS {float(rms):.4f}px > limit {args.max_rms_px:.4f}px"
        ]
        payload.update(
            {
                "status": "READY_FOR_REVIEW" if not blockers else "BLOCKED",
                "blockers": blockers,
                "image_size": {"width": image_size[0], "height": image_size[1]},
                "rms_px": float(rms),
                "per_view_rmse_px": per_view_rmse,
                "camera_matrix": matrix.tolist(),
                "distortion_coefficients": distortion.reshape(-1).tolist(),
            }
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2))
    print(f"wrote {args.out}")
    return 0 if payload["status"] == "READY_FOR_REVIEW" else 1


if __name__ == "__main__":
    raise SystemExit(main())
