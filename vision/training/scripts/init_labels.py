#!/usr/bin/env python3
"""Create / refresh YOLO label sidecars under datasets/raw.

Modes:
  --empty           write empty .txt for every image (real labeling inbox)
  --provisional     write full-frame boxes + mark meta provisional=true (smoke only)
  --from-meta       re-sync labels from meta.jsonl provisional flags

Does NOT treat full-frame as final. Production gate requires real boxes.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import IMAGE_EXTS, RAW  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw", type=Path, default=RAW)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--empty", action="store_true", help="empty label sidecars")
    mode.add_argument(
        "--provisional",
        action="store_true",
        help="full-frame provisional boxes from meta class_id (smoke only)",
    )
    mode.add_argument(
        "--mark-existing-provisional",
        action="store_true",
        help="scan labels; if box is full-frame, set provisional=true in meta",
    )
    args = ap.parse_args()

    img_dir = args.raw / "images"
    lbl_dir = args.raw / "labels"
    meta_path = args.raw / "meta.jsonl"
    lbl_dir.mkdir(parents=True, exist_ok=True)

    meta_rows = []
    if meta_path.is_file():
        for line in meta_path.read_text().splitlines():
            if line.strip():
                meta_rows.append(json.loads(line))
    meta_by_stem = {Path(r["image"]).stem: r for r in meta_rows}

    images = [
        p
        for p in sorted(img_dir.iterdir())
        if p.is_file() and p.suffix.lower() in IMAGE_EXTS and not p.name.startswith("._")
    ]
    n = 0
    for img in images:
        lp = lbl_dir / f"{img.stem}.txt"
        row = meta_by_stem.get(img.stem, {})
        cid = row.get("class_id")

        if args.empty:
            lp.write_text("")
            row["provisional"] = False
            row["provisional_box"] = False
        elif args.provisional:
            if cid is None:
                lp.write_text("")
                row["provisional"] = False
                row["provisional_box"] = False
            else:
                lp.write_text(f"{cid} 0.5 0.5 1.0 1.0\n")
                row["provisional"] = True
                row["provisional_box"] = True
        elif args.mark_existing_provisional:
            if not lp.is_file():
                continue
            text = lp.read_text().strip()
            provisional = False
            for line in text.splitlines():
                parts = line.split()
                if len(parts) != 5:
                    continue
                _, cx, cy, w, h = map(float, parts)
                area = w * h
                if area >= 0.95 or (w >= 0.98 and h >= 0.98):
                    provisional = True
                    break
            row["provisional"] = provisional
            row["provisional_box"] = provisional

        if "image" not in row:
            row["image"] = f"images/{img.name}"
            row["label"] = f"labels/{img.stem}.txt"
        meta_by_stem[img.stem] = row
        n += 1

    # rewrite meta
    with meta_path.open("w", encoding="utf-8") as f:
        for img in images:
            row = meta_by_stem[img.stem]
            f.write(json.dumps(row) + "\n")

    print(f"init_labels: updated {n} labels under {lbl_dir}")
    if args.provisional:
        print("WARNING: provisional full-frame — production QC will FAIL without --allow-provisional")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
