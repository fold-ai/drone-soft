#!/usr/bin/env python3
"""QC YOLO labels under training/datasets/raw.

REJECTS full-frame junk by default (area>=0.95 OR w,h>=0.98) unless
--allow-provisional (smoke only). Also checks class ids, normalized ranges,
missing labels, and meta provisional flags.

Production gate FAILS without real boxes.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import IMAGE_EXTS, RAW, VALID_CLASS_IDS  # noqa: E402

AREA_REJECT = 0.95
WH_REJECT = 0.98


def is_fullframe(w: float, h: float) -> bool:
    return (w * h) >= AREA_REJECT or (w >= WH_REJECT and h >= WH_REJECT)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw", type=Path, default=RAW)
    ap.add_argument("--min-positives", type=int, default=0)
    ap.add_argument(
        "--allow-provisional",
        action="store_true",
        help="SMOKE ONLY: allow full-frame / provisional boxes (production must omit)",
    )
    ap.add_argument("--json-out", type=Path, default=None, help="optional QC report path")
    args = ap.parse_args()

    img_dir = args.raw / "images"
    lbl_dir = args.raw / "labels"
    meta_path = args.raw / "meta.jsonl"

    meta_by_stem: dict[str, dict] = {}
    if meta_path.is_file():
        for line in meta_path.read_text().splitlines():
            if line.strip():
                row = json.loads(line)
                meta_by_stem[Path(row["image"]).stem] = row

    errors: list[str] = []
    warnings: list[str] = []
    n_files = n_pos = n_neg = n_missing = n_fullframe = n_prov_meta = 0

    images = [
        p
        for p in sorted(img_dir.iterdir())
        if p.is_file() and p.suffix.lower() in IMAGE_EXTS and not p.name.startswith("._")
    ] if img_dir.is_dir() else []

    for img in images:
        lp = lbl_dir / f"{img.stem}.txt"
        if not lp.is_file():
            n_missing += 1
            errors.append(f"missing label: {lp.name}")
            continue
        n_files += 1
        text = lp.read_text().strip()
        row = meta_by_stem.get(img.stem, {})
        if row.get("provisional") or row.get("provisional_box"):
            n_prov_meta += 1

        if not text:
            # empty = negative OR unlabeled positive
            if row.get("class_id") is not None and row.get("class") != "negative":
                warnings.append(f"{lp.name}: empty label for positive class {row.get('class')}")
            n_neg += 1
            continue

        n_pos += 1
        for line in text.splitlines():
            parts = line.split()
            if len(parts) != 5:
                errors.append(f"{lp.name}: bad line {line!r}")
                continue
            try:
                cid = int(float(parts[0]))
                cx, cy, w, h = map(float, parts[1:])
            except ValueError:
                errors.append(f"{lp.name}: parse error {line!r}")
                continue
            if cid not in VALID_CLASS_IDS:
                errors.append(f"{lp.name}: class_id {cid} not in {sorted(VALID_CLASS_IDS)}")
            if not all(0.0 <= v <= 1.0 for v in (cx, cy, w, h)):
                errors.append(f"{lp.name}: normalized box out of range {[cx, cy, w, h]}")
            if is_fullframe(w, h):
                n_fullframe += 1
                msg = (
                    f"{lp.name}: full-frame junk "
                    f"(w={w:.3f} h={h:.3f} area={w*h:.3f}) — reject unless --allow-provisional"
                )
                if args.allow_provisional:
                    warnings.append(msg)
                else:
                    errors.append(msg)

    report = {
        "images": len(images),
        "label_files": n_files,
        "positives": n_pos,
        "empty_or_negative": n_neg,
        "missing_labels": n_missing,
        "fullframe_boxes": n_fullframe,
        "provisional_meta": n_prov_meta,
        "allow_provisional": args.allow_provisional,
        "valid_class": sorted(VALID_CLASS_IDS),
        "errors": len(errors),
        "warnings": len(warnings),
        "pass": len(errors) == 0 and n_pos >= args.min_positives and n_missing == 0,
        "production_ready": (
            len(errors) == 0
            and n_fullframe == 0
            and n_prov_meta == 0
            and not args.allow_provisional
        ),
    }
    print(json.dumps(report, indent=2))
    for e in errors[:80]:
        print(f"ERR {e}", file=sys.stderr)
    for w in warnings[:40]:
        print(f"WARN {w}", file=sys.stderr)

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2) + "\n")

    if errors:
        print("label_qc: FAIL (production gate requires real boxes)", file=sys.stderr)
        return 1
    if n_pos < args.min_positives:
        print(f"FAIL: positives {n_pos} < min {args.min_positives}", file=sys.stderr)
        return 1
    if args.allow_provisional and n_fullframe:
        print(
            f"label_qc: PASS (SMOKE --allow-provisional; {n_fullframe} full-frame boxes)",
            file=sys.stderr,
        )
    else:
        print("label_qc: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
