#!/usr/bin/env python3
"""Import Label Studio YOLO export (or JSON tasks with bbox) into datasets/raw.

Preferred: Label Studio "YOLO" export directory with labels/*.txt + images/.
Also accepts --json export with rectanglelabels (normalized percent).

Clears provisional flags for imported stems. Rejects full-frame unless allowed.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import CLASS_NAMES, RAW, VALID_CLASS_IDS  # noqa: E402
from import_yolo_labels import is_fullframe, validate_label  # reuse


NAME_TO_ID = {v: k for k, v in CLASS_NAMES.items()}
# common Label Studio aliases
NAME_TO_ID.update(
    {
        "Shahed-136": 0,
        "shahed-136": 0,
        "Geran-2": 1,
        "geran-2": 1,
        "Gerbera": 2,
        "gerbera": 2,
    }
)


def from_ls_json(tasks: list, raw: Path, allow_provisional: bool, dry_run: bool) -> tuple[int, int]:
    lbl_dir = raw / "labels"
    lbl_dir.mkdir(parents=True, exist_ok=True)
    meta_path = raw / "meta.jsonl"
    meta_rows = []
    if meta_path.is_file():
        meta_rows = [json.loads(l) for l in meta_path.read_text().splitlines() if l.strip()]
    by_stem = {Path(r["image"]).stem: r for r in meta_rows}

    imported = skipped = 0
    for task in tasks:
        data = task.get("data") or {}
        img_path = data.get("image") or data.get("img") or ""
        stem = Path(str(img_path)).stem
        # find matching dataset stem
        matches = [s for s in by_stem if s == stem or s.endswith(f"__{stem}")]
        if not matches:
            # try filename without URL query
            stem2 = Path(str(img_path).split("?")[0]).stem
            matches = [s for s in by_stem if s == stem2 or s.endswith(f"__{stem2}")]
        if not matches:
            print(f"skip unmatched task image={img_path}")
            skipped += 1
            continue

        lines = []
        for ann in task.get("annotations") or []:
            for r in ann.get("result") or []:
                if r.get("type") not in ("rectanglelabels", "rectangle"):
                    continue
                val = r.get("value") or {}
                labels = val.get("rectanglelabels") or val.get("labels") or []
                if not labels:
                    continue
                name = labels[0]
                cid = NAME_TO_ID.get(name)
                if cid is None:
                    print(f"unknown label {name!r}")
                    continue
                # LS percent of image
                x = float(val["x"]) / 100.0
                y = float(val["y"]) / 100.0
                w = float(val["width"]) / 100.0
                h = float(val["height"]) / 100.0
                cx = x + w / 2.0
                cy = y + h / 2.0
                if is_fullframe(w, h) and not allow_provisional:
                    print(f"REJECT full-frame for {stem}")
                    continue
                lines.append(f"{cid} {cx:.6f} {cy:.6f} {w:.6f} {h:.6f}")

        text = "\n".join(lines) + ("\n" if lines else "")
        errs = validate_label(text, allow_provisional) if lines else []
        if errs:
            print(f"REJECT {stem}: {errs}")
            skipped += 1
            continue
        for m in matches:
            dst = lbl_dir / f"{m}.txt"
            if not dry_run:
                dst.write_text(text)
                if m in by_stem:
                    by_stem[m]["provisional"] = False
                    by_stem[m]["provisional_box"] = False
            imported += 1
            print(f"LS import -> {dst.name} ({len(lines)} boxes)")

    if not dry_run and meta_rows:
        with meta_path.open("w", encoding="utf-8") as f:
            for r in meta_rows:
                stem = Path(r["image"]).stem
                f.write(json.dumps(by_stem.get(stem, r)) + "\n")
    return imported, skipped


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw", type=Path, default=RAW)
    ap.add_argument("--yolo-dir", type=Path, help="Label Studio YOLO export root")
    ap.add_argument("--json", type=Path, help="Label Studio JSON export")
    ap.add_argument("--allow-provisional", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    if args.yolo_dir:
        # delegate to import_yolo_labels
        from import_yolo_labels import main as yolo_main
        import sys as _sys

        _sys.argv = [
            "import_yolo_labels.py",
            "--raw",
            str(args.raw),
            "--from-dir",
            str(args.yolo_dir),
        ]
        if args.allow_provisional:
            _sys.argv.append("--allow-provisional")
        if args.dry_run:
            _sys.argv.append("--dry-run")
        return yolo_main()

    if args.json:
        tasks = json.loads(args.json.read_text())
        if isinstance(tasks, dict):
            tasks = tasks.get("tasks") or [tasks]
        imported, skipped = from_ls_json(tasks, args.raw, args.allow_provisional, args.dry_run)
        print(f"import_labelstudio: imported={imported} skipped={skipped}")
        return 0 if imported else 1

    print("provide --yolo-dir or --json", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
