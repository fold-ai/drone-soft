#!/usr/bin/env python3
"""Standard-library integrity audit for YOLO image/label split manifests.

This checks reproducibility and leakage only; it does not claim semantic label
quality and does not train or enable a detector.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def label_path(dataset_root: Path, image_ref: str) -> Path:
    relative = Path(image_ref)
    parts = list(relative.parts)
    if parts and parts[0] == "images":
        parts[0] = "labels"
    return (dataset_root / Path(*parts)).with_suffix(".txt")


def parse_label(path: Path) -> tuple[list[int], list[str]]:
    classes: list[int] = []
    errors: list[str] = []
    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw.strip():
            continue
        fields = raw.split()
        if len(fields) != 5:
            errors.append(f"{path}:{line_no}: expected 5 fields")
            continue
        try:
            class_id = int(fields[0])
            coords = [float(value) for value in fields[1:]]
        except ValueError:
            errors.append(f"{path}:{line_no}: non-numeric field")
            continue
        if class_id < 0:
            errors.append(f"{path}:{line_no}: negative class id")
        if any(value < 0.0 or value > 1.0 for value in coords):
            errors.append(f"{path}:{line_no}: coordinate outside [0,1]")
        if coords[2] <= 0.0 or coords[3] <= 0.0:
            errors.append(f"{path}:{line_no}: box width/height must be positive")
        classes.append(class_id)
    return classes, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-root", type=Path, default=ROOT / "vision/data/raw")
    parser.add_argument("--manifests", type=Path, default=ROOT / "vision/data/manifests")
    parser.add_argument("--splits", default="train,val,test")
    parser.add_argument("--min-images", type=int, default=200)
    parser.add_argument("--min-per-class", type=int, default=50)
    parser.add_argument("--min-negative-frac", type=float, default=0.10)
    parser.add_argument(
        "--out",
        type=Path,
        default=ROOT / "vision/training/exports/dataset_integrity_audit.json",
    )
    parser.add_argument("--require-ready", action="store_true")
    args = parser.parse_args()

    splits = [item.strip() for item in args.splits.split(",") if item.strip()]
    blockers: list[str] = []
    split_counts: dict[str, int] = {}
    class_counts: Counter[int] = Counter()
    negative_count = 0
    missing_images: list[str] = []
    missing_labels: list[str] = []
    label_errors: list[str] = []
    digest_splits: dict[str, set[str]] = defaultdict(set)
    path_splits: dict[str, set[str]] = defaultdict(set)
    total_valid = 0

    for split in splits:
        manifest = args.manifests / f"{split}.txt"
        if not manifest.is_file():
            blockers.append(f"missing manifest: {manifest}")
            split_counts[split] = 0
            continue
        refs = [line.strip() for line in manifest.read_text(encoding="utf-8").splitlines() if line.strip()]
        split_counts[split] = len(refs)
        for ref in refs:
            path_splits[ref].add(split)
            image = args.dataset_root / ref
            label = label_path(args.dataset_root, ref)
            if not image.is_file():
                missing_images.append(f"{split}:{ref}")
                continue
            digest_splits[sha256(image)].add(split)
            if not label.is_file():
                missing_labels.append(f"{split}:{label.relative_to(args.dataset_root)}")
                continue
            classes, errors = parse_label(label)
            label_errors.extend(errors)
            if not classes:
                negative_count += 1
            class_counts.update(classes)
            total_valid += 1

    path_leaks = sorted(ref for ref, owners in path_splits.items() if len(owners) > 1)
    hash_leaks = sorted(digest for digest, owners in digest_splits.items() if len(owners) > 1)
    negative_fraction = negative_count / total_valid if total_valid else 0.0

    if missing_images:
        blockers.append(f"missing images: {len(missing_images)}")
    if missing_labels:
        blockers.append(f"missing labels: {len(missing_labels)}")
    if label_errors:
        blockers.append(f"invalid label rows: {len(label_errors)}")
    if path_leaks:
        blockers.append(f"same path appears across splits: {len(path_leaks)}")
    if hash_leaks:
        blockers.append(f"same image bytes appear across splits: {len(hash_leaks)}")
    if total_valid < args.min_images:
        blockers.append(f"valid image/label pairs {total_valid} < required {args.min_images}")
    for class_id, count in sorted(class_counts.items()):
        if count < args.min_per_class:
            blockers.append(f"class {class_id} boxes {count} < required {args.min_per_class}")
    if total_valid and negative_fraction < args.min_negative_frac:
        blockers.append(
            f"negative fraction {negative_fraction:.3f} < required {args.min_negative_frac:.3f}"
        )

    report = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "scope": "dataset_integrity_not_semantic_quality",
        "status": "READY_FOR_INDEPENDENT_REVIEW" if not blockers else "BLOCKED",
        "blockers": blockers,
        "dataset_root": str(args.dataset_root),
        "manifests": str(args.manifests),
        "split_entries": split_counts,
        "valid_image_label_pairs": total_valid,
        "class_box_counts": {str(key): value for key, value in sorted(class_counts.items())},
        "negative_images": negative_count,
        "negative_fraction": negative_fraction,
        "missing_images": missing_images[:100],
        "missing_labels": missing_labels[:100],
        "label_errors": label_errors[:100],
        "path_split_leaks": path_leaks[:100],
        "hash_split_leaks": hash_leaks[:100],
        "note": "A passing integrity audit still requires independent human label and domain review.",
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"wrote {args.out}")
    return 4 if args.require_ready and blockers else 0


if __name__ == "__main__":
    raise SystemExit(main())
