#!/usr/bin/env python3
"""Import real YOLO .txt boxes into training/datasets/raw/labels/.

Source may be:
  --from-dir DIR     flat or nested .txt files matched by stem to images/
  --from-zip ZIP     extract then match stems

Clears provisional flags in meta for successfully imported stems.
Rejects full-frame imports unless --allow-provisional.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import RAW, VALID_CLASS_IDS  # noqa: E402

AREA_REJECT = 0.95
WH_REJECT = 0.98


def is_fullframe(w: float, h: float) -> bool:
    return (w * h) >= AREA_REJECT or (w >= WH_REJECT and h >= WH_REJECT)


def validate_label(text: str, allow_provisional: bool) -> list[str]:
    errs = []
    for line in text.strip().splitlines():
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) != 5:
            errs.append(f"bad line {line!r}")
            continue
        cid = int(float(parts[0]))
        cx, cy, w, h = map(float, parts[1:])
        if cid not in VALID_CLASS_IDS:
            errs.append(f"class_id {cid} not gated")
        if not all(0.0 <= v <= 1.0 for v in (cx, cy, w, h)):
            errs.append(f"out of range {[cx, cy, w, h]}")
        if is_fullframe(w, h) and not allow_provisional:
            errs.append(f"full-frame rejected w={w} h={h}")
    return errs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw", type=Path, default=RAW)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--from-dir", type=Path)
    src.add_argument("--from-zip", type=Path)
    ap.add_argument("--allow-provisional", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    lbl_dir = args.raw / "labels"
    meta_path = args.raw / "meta.jsonl"
    lbl_dir.mkdir(parents=True, exist_ok=True)

    tmp = None
    if args.from_zip:
        tmp = Path(tempfile.mkdtemp(prefix="yolo_import_"))
        with zipfile.ZipFile(args.from_zip) as zf:
            zf.extractall(tmp)
        root = tmp
    else:
        root = args.from_dir

    txts = {p.stem: p for p in root.rglob("*.txt") if p.is_file()}
    meta_rows = []
    if meta_path.is_file():
        meta_rows = [json.loads(l) for l in meta_path.read_text().splitlines() if l.strip()]
    by_stem = {Path(r["image"]).stem: r for r in meta_rows}

    imported = skipped = 0
    for stem, src_txt in sorted(txts.items()):
        # Match exact stem or class__stem variants
        targets = [stem]
        if stem not in by_stem:
            targets = [s for s in by_stem if s == stem or s.endswith(f"__{stem}")]
        if not targets and stem in {Path(p).stem for p in (args.raw / "images").glob("*")}:
            targets = [stem]
        if not targets:
            print(f"skip unmatched: {src_txt.name}")
            skipped += 1
            continue
        text = src_txt.read_text()
        errs = validate_label(text, args.allow_provisional)
        if errs:
            print(f"REJECT {src_txt.name}: {errs}")
            skipped += 1
            continue
        for t in targets:
            dst = lbl_dir / f"{t}.txt"
            if not args.dry_run:
                shutil.copy2(src_txt, dst)
                if t in by_stem:
                    by_stem[t]["provisional"] = False
                    by_stem[t]["provisional_box"] = False
            imported += 1
            print(f"import -> {dst.name}")

    if not args.dry_run and meta_rows:
        # preserve order from images if possible
        with meta_path.open("w", encoding="utf-8") as f:
            for r in meta_rows:
                stem = Path(r["image"]).stem
                f.write(json.dumps(by_stem.get(stem, r)) + "\n")

    if tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"import_yolo_labels: imported={imported} skipped={skipped}")
    return 0 if imported else 1


if __name__ == "__main__":
    raise SystemExit(main())
