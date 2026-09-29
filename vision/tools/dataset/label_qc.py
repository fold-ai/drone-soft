#!/usr/bin/env python3
"""QC labels under data/raw: class ids, empty boxes, provisional rate, EO cam_id."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# V1 range-test primary. Legacy 10/11 accepted in QC if present but flagged.
VALID_CLASS = {0, 1, 2}
LEGACY_CLASS = {10, 11}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--labels", type=Path, default=Path("data/raw"), help="raw root with labels/ + meta.jsonl")
    ap.add_argument("--min-positives", type=int, default=0)
    ap.add_argument("--allow-legacy", action="store_true", help="accept legacy class ids 10/11")
    args = ap.parse_args()

    lbl_dir = args.labels / "labels" if (args.labels / "labels").is_dir() else args.labels
    meta_path = args.labels / "meta.jsonl"
    accepted = set(VALID_CLASS)
    if args.allow_legacy:
        accepted |= LEGACY_CLASS

    errors = []
    n_files = n_pos = n_neg = n_prov = n_legacy = 0
    for lp in sorted(lbl_dir.glob("*.txt")):
        n_files += 1
        text = lp.read_text().strip()
        if not text:
            n_neg += 1
            continue
        n_pos += 1
        for line in text.splitlines():
            parts = line.split()
            if len(parts) != 5:
                errors.append(f"{lp.name}: bad line {line!r}")
                continue
            cid = int(float(parts[0]))
            if cid in LEGACY_CLASS:
                n_legacy += 1
            if cid not in accepted:
                errors.append(f"{lp.name}: class_id {cid} not in {sorted(accepted)}")
            vals = list(map(float, parts[1:]))
            if not all(0.0 <= v <= 1.0 for v in vals):
                errors.append(f"{lp.name}: normalized box out of range {vals}")

    if meta_path.is_file():
        for line in meta_path.read_text().splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("cam_id", 0) != 0:
                errors.append(f"meta cam_id!=0 (V1 EO-only): {row.get('image')}")
            if row.get("provisional_box"):
                n_prov += 1

    report = {
        "label_files": n_files,
        "positives": n_pos,
        "negatives": n_neg,
        "provisional_boxes": n_prov,
        "legacy_class_labels": n_legacy,
        "valid_class": sorted(VALID_CLASS),
        "errors": len(errors),
        "pass": len(errors) == 0 and n_pos >= args.min_positives,
    }
    print(json.dumps(report, indent=2))
    for e in errors[:50]:
        print(f"ERR {e}", file=sys.stderr)
    if errors:
        return 1
    if n_pos < args.min_positives:
        print(f"FAIL: positives {n_pos} < min {args.min_positives}", file=sys.stderr)
        return 1
    print("label_qc: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
