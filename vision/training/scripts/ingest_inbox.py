#!/usr/bin/env python3
"""Ingest vision/data/inbox → training/datasets/raw/ (images + labels + meta).

Primary class folders: shahed_136, geran_2, gerbera, negative.
Does NOT write final boxes — calls init_labels logic for provisional sidecars
marked provisional=true. Production QC rejects full-frame unless --allow-provisional.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import CLASSES, IMAGE_EXTS, INBOX, RAW  # noqa: E402


def iter_images(folder: Path):
    if not folder.is_dir():
        return
    for p in sorted(folder.rglob("*")):
        if p.is_file() and p.suffix.lower() in IMAGE_EXTS and not p.name.startswith("._"):
            yield p


def link_or_copy(src: Path, dst: Path, copy: bool) -> None:
    if copy:
        shutil.copy2(src, dst)
        return
    try:
        dst.hardlink_to(src)
    except OSError:
        try:
            dst.symlink_to(src.resolve())
        except OSError:
            shutil.copy2(src, dst)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--inbox", type=Path, default=INBOX)
    ap.add_argument("--out", type=Path, default=RAW)
    ap.add_argument("--copy", action="store_true", help="copy files instead of hardlink/symlink")
    ap.add_argument(
        "--provisional-fullframe",
        action="store_true",
        default=True,
        help="write provisional full-frame YOLO boxes (default; marked provisional=true)",
    )
    ap.add_argument(
        "--empty-labels",
        action="store_true",
        help="write empty label sidecars instead of provisional full-frame",
    )
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
                link_or_copy(src, dst, args.copy)

                label_path = lbl_dir / f"{dst.stem}.txt"
                provisional = False
                if class_id is None:
                    label_path.write_text("")
                    n_neg += 1
                    cid = None
                elif args.empty_labels:
                    label_path.write_text("")
                    n_pos += 1
                    cid = class_id
                    provisional = False  # empty = unlabeled, not provisional box
                else:
                    # Provisional full-frame — NOT final; QC rejects by default
                    label_path.write_text(f"{class_id} 0.5 0.5 1.0 1.0\n")
                    n_pos += 1
                    cid = class_id
                    provisional = True

                per_class[class_name] = per_class.get(class_name, 0) + 1
                meta.write(
                    json.dumps(
                        {
                            "image": str(dst.relative_to(args.out)),
                            "label": str(label_path.relative_to(args.out)),
                            "class": class_name,
                            "class_id": cid,
                            "cam_id": 0,
                            "source": str(src),
                            "provisional": provisional,
                            "provisional_box": provisional,
                        }
                    )
                    + "\n"
                )

    print(f"ingest_inbox: positives={n_pos} negatives={n_neg} -> {args.out}")
    for k, v in sorted(per_class.items()):
        print(f"  {k}: {v}")
    if n_pos == 0:
        print("warning: no positives in inbox", file=sys.stderr)
        return 2
    print("NOTE: provisional full-frame labels are NOT production-ready.")
    print("      Run label_qc.py (rejects full-frame unless --allow-provisional).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
