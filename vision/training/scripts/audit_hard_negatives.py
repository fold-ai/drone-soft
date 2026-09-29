#!/usr/bin/env python3
"""Audit operator-captured hard negatives for the passive Nose EO demo.

The report is intentionally separate from the class-gated recognition gate.
It validates local negative samples and can measure ordinary detector emissions
at several confidence thresholds. It never trains, exports, or enables a model.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import statistics
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from _paths import EXPORTS, IMAGE_EXTS, INBOX  # noqa: E402

DEFAULT_NEGATIVE_DIR = INBOX / "negative"
DEFAULT_OUT = EXPORTS / "hard_negative_audit.json"


def parse_thresholds(raw: str) -> list[float]:
    values = sorted({float(item.strip()) for item in raw.split(",") if item.strip()})
    if not values or any(value <= 0.0 or value >= 1.0 for value in values):
        raise ValueError("thresholds must be comma-separated values between 0 and 1")
    return values


def load_capture_metadata(folder: Path) -> dict[str, dict[str, Any]]:
    path = folder / "capture_meta.jsonl"
    rows: dict[str, dict[str, Any]] = {}
    if not path.is_file():
        return rows
    for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            rows[f"__invalid_line_{line_no}"] = {"error": str(exc)}
            continue
        image = str(row.get("image", ""))
        if image:
            rows[image] = row
    return rows


def inspect_samples(folder: Path) -> tuple[list[dict[str, Any]], list[str]]:
    try:
        import cv2
    except ImportError as exc:
        raise RuntimeError("opencv is required to validate negative images") from exc

    metadata = load_capture_metadata(folder)
    samples: list[dict[str, Any]] = []
    corrupt: list[str] = []
    if not folder.is_dir():
        return samples, corrupt

    for path in sorted(folder.iterdir()):
        if not path.is_file() or path.suffix.lower() not in IMAGE_EXTS:
            continue
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        image = cv2.imread(str(path), cv2.IMREAD_COLOR)
        if image is None:
            corrupt.append(path.name)
            samples.append(
                {
                    "image": path.name,
                    "sha256": digest,
                    "bytes": len(data),
                    "valid": False,
                    "width": None,
                    "height": None,
                    "reason": metadata.get(path.name, {}).get("reason", "unknown"),
                    "session_id": metadata.get(path.name, {}).get("session_id", f"file:{path.name}"),
                }
            )
            continue
        height, width = image.shape[:2]
        row = metadata.get(path.name, {})
        samples.append(
            {
                "image": path.name,
                "sha256": digest,
                "bytes": len(data),
                "valid": True,
                "width": int(width),
                "height": int(height),
                "reason": str(row.get("reason", "unknown")),
                "cue_class": str(row.get("cue", {}).get("class", "")),
                "session_id": str(row.get("session_id") or f"file:{path.name}"),
            }
        )
    return samples, corrupt


def evaluate_detector(
    images: list[Path], weights: Path, thresholds: list[float], imgsz: int, device: str
) -> dict[str, Any]:
    from ultralytics import YOLO

    model = YOLO(str(weights))
    minimum = min(thresholds)
    per_image: list[list[tuple[str, float]]] = []
    for image in images:
        predictions: list[tuple[str, float]] = []
        results = model.predict(
            source=str(image), conf=minimum, imgsz=imgsz, device=device, verbose=False
        )
        for result in results:
            if result.boxes is None:
                continue
            names = result.names
            for cls_id, confidence in zip(
                result.boxes.cls.detach().cpu().tolist(),
                result.boxes.conf.detach().cpu().tolist(),
            ):
                class_index = int(cls_id)
                if isinstance(names, dict):
                    class_name = names.get(class_index, class_index)
                elif 0 <= class_index < len(names):
                    class_name = names[class_index]
                else:
                    class_name = class_index
                predictions.append((str(class_name), float(confidence)))
        per_image.append(predictions)

    threshold_reports: dict[str, Any] = {}
    for threshold in thresholds:
        image_fp = 0
        detection_count = 0
        class_counts: Counter[str] = Counter()
        for predictions in per_image:
            selected = [(name, conf) for name, conf in predictions if conf >= threshold]
            if selected:
                image_fp += 1
            detection_count += len(selected)
            class_counts.update(name for name, _ in selected)
        total = len(images)
        threshold_reports[f"{threshold:.2f}"] = {
            "images_with_detection": image_fp,
            "image_false_positive_rate": image_fp / total if total else None,
            "detections": detection_count,
            "mean_detections_per_image": detection_count / total if total else None,
            "classes": dict(class_counts.most_common()),
        }
    return {
        "weights": str(weights),
        "imgsz": imgsz,
        "device": device,
        "thresholds": threshold_reports,
    }


def build_report(
    folder: Path,
    *,
    min_samples: int,
    min_sessions: int,
    weights: Path | None,
    thresholds: list[float],
    imgsz: int,
    device: str,
) -> dict[str, Any]:
    samples, corrupt = inspect_samples(folder)
    valid = [sample for sample in samples if sample["valid"]]
    hashes = Counter(sample["sha256"] for sample in valid)
    duplicate_files = sum(count - 1 for count in hashes.values() if count > 1)
    reason_counts = Counter(str(sample["reason"]) for sample in valid)
    cue_counts = Counter(str(sample["cue_class"]) for sample in valid if sample["cue_class"])
    session_counts = Counter(str(sample["session_id"]) for sample in valid)
    widths = [int(sample["width"]) for sample in valid]
    heights = [int(sample["height"]) for sample in valid]

    blockers: list[str] = []
    if len(valid) < min_samples:
        blockers.append(f"valid samples {len(valid)} < required {min_samples}")
    if len(session_counts) < min_sessions:
        blockers.append(f"capture sessions {len(session_counts)} < required {min_sessions}")
    if corrupt:
        blockers.append(f"corrupt images: {len(corrupt)}")

    report: dict[str, Any] = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "scope": "passive_nose_eo_hard_negatives",
        "status": "READY" if not blockers else "BLOCKED",
        "blockers": blockers,
        "folder": str(folder),
        "minimum_samples": min_samples,
        "minimum_sessions": min_sessions,
        "samples": {
            "files": len(samples),
            "valid": len(valid),
            "corrupt": len(corrupt),
            "duplicates": duplicate_files,
            "reasons": dict(reason_counts.most_common()),
            "cue_classes": dict(cue_counts.most_common()),
            "capture_sessions": len(session_counts),
            "frames_per_session": dict(session_counts.most_common()),
            "median_width": statistics.median(widths) if widths else None,
            "median_height": statistics.median(heights) if heights else None,
            "corrupt_files": corrupt,
        },
        "detector_eval": None,
        "note": "Offline audit only; this report does not enable tracking or flight control.",
    }

    if weights is not None:
        if not weights.is_file():
            report["blockers"].append(f"missing weights: {weights}")
            report["status"] = "BLOCKED"
        elif valid:
            report["detector_eval"] = evaluate_detector(
                [folder / sample["image"] for sample in valid],
                weights,
                thresholds,
                imgsz,
                device,
            )
        else:
            report["detector_eval"] = {
                "weights": str(weights),
                "thresholds": {},
                "note": "Not run: no valid hard-negative samples.",
            }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--negative-dir", type=Path, default=DEFAULT_NEGATIVE_DIR)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--min-samples", type=int, default=50)
    parser.add_argument("--min-sessions", type=int, default=5)
    parser.add_argument("--weights", type=Path, default=None)
    parser.add_argument("--thresholds", default="0.25,0.35,0.50")
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--device", default="cpu")
    parser.add_argument(
        "--require-ready",
        action="store_true",
        help="exit non-zero when sample readiness is blocked",
    )
    args = parser.parse_args()
    try:
        thresholds = parse_thresholds(args.thresholds)
        report = build_report(
            args.negative_dir,
            min_samples=max(1, args.min_samples),
            min_sessions=max(1, args.min_sessions),
            weights=args.weights,
            thresholds=thresholds,
            imgsz=args.imgsz,
            device=args.device,
        )
    except (RuntimeError, ValueError) as exc:
        print(f"audit_hard_negatives: {exc}", file=sys.stderr)
        return 2

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    print(f"wrote {args.out}")
    return 4 if args.require_ready and report["status"] != "READY" else 0


if __name__ == "__main__":
    raise SystemExit(main())
