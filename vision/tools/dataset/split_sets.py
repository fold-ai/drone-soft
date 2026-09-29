#!/usr/bin/env python3
"""Train/val/test manifests from data/raw.

For still-photo inbox (Shahed-136_0001.jpg style), each file is its own split unit
(full stem as key). Only strips ingest collision suffix __stem_N when N is a
duplicate counter from ingest_raw — not the source frame number.

Writes data/manifests/{train,val,test}.txt as image paths relative to raw root.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def seq_key(image_name: str) -> str:
    """Stable key per still. Do not collapse Class_0001..Class_0NNN into one clip."""
    stem = Path(image_name).stem
    # Ingest collision only: foo_1, foo_2 when foo already existed — rare.
    # Source names like Geran-2_0001 must stay distinct → use full stem.
    return stem


def bucket(key: str, val_frac: float, test_frac: float) -> str:
    h = int(hashlib.md5(key.encode()).hexdigest(), 16) % 1000
    t = int(test_frac * 1000)
    v = int(val_frac * 1000)
    if h < t:
        return "test"
    if h < t + v:
        return "val"
    return "train"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("data/raw"))
    ap.add_argument("--out", type=Path, default=Path("data/manifests"))
    ap.add_argument("--val-frac", type=float, default=0.15)
    ap.add_argument("--test-frac", type=float, default=0.15)
    args = ap.parse_args()

    img_dir = args.root / "images"
    meta_path = args.root / "meta.jsonl"
    args.out.mkdir(parents=True, exist_ok=True)

    images = []
    if meta_path.is_file():
        for line in meta_path.read_text().splitlines():
            if line.strip():
                rel = json.loads(line)["image"]
                # skip appledouble / hidden
                if Path(rel).name.startswith("."):
                    continue
                images.append(rel)
    else:
        images = [
            str(p.relative_to(args.root))
            for p in sorted(img_dir.glob("*"))
            if p.is_file() and not p.name.startswith(".")
        ]

    splits = {"train": [], "val": [], "test": []}
    for rel in images:
        splits[bucket(seq_key(Path(rel).name), args.val_frac, args.test_frac)].append(rel)

    # Guarantee non-empty val for YOLO
    if not splits["val"] and len(splits["train"]) > 1:
        n = max(1, int(len(splits["train"]) * args.val_frac))
        splits["val"] = splits["train"][:n]
        splits["train"] = splits["train"][n:]
        print(f"note: moved {n} train→val (hash left val empty)")

    for name, rows in splits.items():
        (args.out / f"{name}.txt").write_text("\n".join(rows) + ("\n" if rows else ""))
        print(f"{name}: {len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
