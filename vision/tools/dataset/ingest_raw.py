#!/usr/bin/env python3
"""Ingest user-supplied EO target photos from a class inbox into data/raw.

Expected inbox (V1 range-test primary):
  <inbox>/shahed_136/*
  <inbox>/geran_2/*
  <inbox>/gerbera/*
  <inbox>/negative/*   (optional; no box)

Legacy optional folders (documented, still ingestable if present):
  <inbox>/quad/*
  <inbox>/fixed_wing/*

Writes images/, labels/ (YOLO txt), and meta.jsonl with cam_id=0 (EO-only V1).
Unlabeled positives get a provisional full-frame box of that class.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

# V1 range-test primary; legacy kept for optional ingest only.
CLASSES = {
    "shahed_136": 0,
    "geran_2": 1,
    "gerbera": 2,
    "quad": 10,        # legacy / optional — NOT in V1 range-test Lock gate
    "fixed_wing": 11,  # legacy / optional
    "negative": None,
}
EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}


def iter_images(folder: Path):
    if not folder.is_dir():
        return
    for p in sorted(folder.rglob("*")):
        if p.is_file() and p.suffix.lower() in EXTS and not p.name.startswith("._"):
            yield p


def write_yolo_fullframe(label_path: Path, class_id: int) -> None:
    # provisional: whole image is the object (cx,cy,w,h normalized)
    label_path.write_text(f"{class_id} 0.5 0.5 1.0 1.0\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--inbox", type=Path, default=Path("data/inbox"))
    ap.add_argument("--out", type=Path, default=Path("data/raw"))
    ap.add_argument("--copy", action="store_true", help="copy instead of hardlink/symlink attempt")
    args = ap.parse_args()

    img_dir = args.out / "images"
    lbl_dir = args.out / "labels"
    img_dir.mkdir(parents=True, exist_ok=True)
    lbl_dir.mkdir(parents=True, exist_ok=True)
    meta_path = args.out / "meta.jsonl"

    n_pos = n_neg = 0
    per_class: dict[str, int] = {}
    with meta_path.open("w", encoding="utf-8") as meta:
        for class_name, class_id in CLASSES.items():
            src_dir = args.inbox / class_name
            for src in iter_images(src_dir):
                stem = f"{class_name}__{src.stem}"
                dst = img_dir / f"{stem}{src.suffix.lower()}"
                i = 1
                while dst.exists():
                    dst = img_dir / f"{stem}_{i}{src.suffix.lower()}"
                    i += 1
                if args.copy:
                    shutil.copy2(src, dst)
                else:
                    try:
                        dst.hardlink_to(src)
                    except OSError:
                        try:
                            dst.symlink_to(src.resolve())
                        except OSError:
                            shutil.copy2(src, dst)

                label_path = lbl_dir / f"{dst.stem}.txt"
                if class_id is None:
                    label_path.write_text("")  # negative
                    n_neg += 1
                    cid = None
                else:
                    write_yolo_fullframe(label_path, class_id)
                    n_pos += 1
                    cid = class_id

                per_class[class_name] = per_class.get(class_name, 0) + 1
                meta.write(
                    json.dumps(
                        {
                            "image": str(dst.relative_to(args.out)),
                            "label": str(label_path.relative_to(args.out)),
                            "class": class_name,
                            "class_id": cid,
                            "cam_id": 0,  # EO-only V1
                            "source": str(src),
                            "provisional_box": class_id is not None,
                        }
                    )
                    + "\n"
                )

    print(f"ingest_raw: positives={n_pos} negatives={n_neg} -> {args.out}")
    for k, v in sorted(per_class.items()):
        print(f"  {k}: {v}")
    if n_pos == 0:
        print(
            "warning: no positives; drop photos under inbox/shahed_136, geran_2, gerbera",
            file=sys.stderr,
        )
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
