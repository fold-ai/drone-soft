#!/usr/bin/env python3
"""Sequence-safe train/val/test manifests → datasets/manifests/{train,val,test}.txt.

Still-photo inbox: each file is its own split unit (full stem).
Hash-bucket split; guarantees non-empty val when possible.
Optional --smoke-per-class N writes datasets/smoke/{train,val,test}.txt subset.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import defaultdict
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import IMAGE_EXTS, MANIFESTS, RAW, SMOKE  # noqa: E402


def seq_key(image_name: str) -> str:
    return Path(image_name).stem


def bucket(key: str, val_frac: float, test_frac: float) -> str:
    h = int(hashlib.md5(key.encode()).hexdigest(), 16) % 1000
    t = int(test_frac * 1000)
    v = int(val_frac * 1000)
    if h < t:
        return "test"
    if h < t + v:
        return "val"
    return "train"


def load_images(root: Path) -> list[tuple[str, str | None]]:
    """Return list of (rel_image_path, class_name)."""
    meta_path = root / "meta.jsonl"
    rows = []
    if meta_path.is_file():
        for line in meta_path.read_text().splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            rel = row["image"]
            if Path(rel).name.startswith("."):
                continue
            rows.append((rel, row.get("class")))
        return rows
    img_dir = root / "images"
    for p in sorted(img_dir.iterdir()):
        if p.is_file() and p.suffix.lower() in IMAGE_EXTS and not p.name.startswith("."):
            rows.append((str(p.relative_to(root)), None))
    return rows


def write_splits(out: Path, splits: dict[str, list[str]]) -> None:
    out.mkdir(parents=True, exist_ok=True)
    for name, rows in splits.items():
        (out / f"{name}.txt").write_text("\n".join(rows) + ("\n" if rows else ""))
        print(f"{out.name}/{name}: {len(rows)}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=RAW)
    ap.add_argument("--out", type=Path, default=MANIFESTS)
    ap.add_argument("--val-frac", type=float, default=0.15)
    ap.add_argument("--test-frac", type=float, default=0.15)
    ap.add_argument(
        "--smoke-per-class",
        type=int,
        default=0,
        help="also write smoke subset with N images per gated class (0=skip)",
    )
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    images = load_images(args.root)
    splits: dict[str, list[str]] = {"train": [], "val": [], "test": []}
    for rel, _cls in images:
        splits[bucket(seq_key(Path(rel).name), args.val_frac, args.test_frac)].append(rel)

    if not splits["val"] and len(splits["train"]) > 1:
        n = max(1, int(len(splits["train"]) * args.val_frac))
        splits["val"] = splits["train"][:n]
        splits["train"] = splits["train"][n:]
        print(f"note: moved {n} train→val (hash left val empty)")

    write_splits(args.out, splits)

    if args.smoke_per_class > 0:
        rng = random.Random(args.seed)
        by_class: dict[str, list[str]] = defaultdict(list)
        for rel, cls in images:
            if cls in ("shahed_136", "geran_2", "gerbera"):
                by_class[cls].append(rel)
        smoke_pool = []
        for cls, items in sorted(by_class.items()):
            take = min(args.smoke_per_class, len(items))
            chosen = rng.sample(items, take)
            smoke_pool.extend(chosen)
            print(f"smoke sample {cls}: {take}")
        smoke_splits: dict[str, list[str]] = {"train": [], "val": [], "test": []}
        for rel in smoke_pool:
            smoke_splits[bucket(seq_key(Path(rel).name), args.val_frac, args.test_frac)].append(rel)
        if not smoke_splits["val"] and len(smoke_splits["train"]) > 1:
            n = max(1, len(smoke_splits["train"]) // 5)
            smoke_splits["val"] = smoke_splits["train"][:n]
            smoke_splits["train"] = smoke_splits["train"][n:]
        if not smoke_splits["train"] and smoke_splits["val"]:
            # ensure train non-empty for YOLO
            smoke_splits["train"] = smoke_splits["val"][:1]
            smoke_splits["val"] = smoke_splits["val"][1:] or smoke_splits["train"]
        write_splits(SMOKE, smoke_splits)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
