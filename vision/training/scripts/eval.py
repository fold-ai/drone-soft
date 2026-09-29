#!/usr/bin/env python3
"""Evaluate a trained run: mAP50, mAP50-95, per-class AP, FP rate on negatives.

Writes summary to stdout and optionally merges into metrics.json via write_metrics_json.
If no negatives exist, fp_on_negatives is null / N/A with count 0.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import CLASS_NAMES, CONFIGS, MANIFESTS, RAW, RUNS, SMOKE  # noqa: E402


def count_negatives(raw: Path, manifests: Path, split: str = "val") -> list[Path]:
    """Images with empty labels (negatives) listed in split manifest."""
    split_file = manifests / f"{split}.txt"
    if not split_file.is_file():
        return []
    negs = []
    for line in split_file.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        img = raw / line
        stem = img.stem
        lbl = raw / "labels" / f"{stem}.txt"
        if lbl.is_file() and not lbl.read_text().strip():
            # also check meta class
            negs.append(img)
    # also scan all empty labels if meta says negative
    meta = raw / "meta.jsonl"
    if meta.is_file():
        neg_stems = set()
        for line in meta.read_text().splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("class") == "negative" or row.get("class_id") is None:
                if not (raw / "labels" / f"{Path(row['image']).stem}.txt").read_text().strip():
                    neg_stems.add(Path(row["image"]).stem)
        # prefer full negative set for FP if present
        if neg_stems:
            negs = [raw / "images" / p.name for p in (raw / "images").iterdir() if p.stem in neg_stems]
    return [p for p in negs if p.is_file()]


def fp_rate_on_negatives(weights: Path, neg_images: list[Path], conf: float, imgsz: int, device: str) -> dict:
    if not neg_images:
        return {
            "fp_on_negatives": None,
            "fp_on_negatives_note": "N/A — no negatives in dataset (count=0)",
            "negatives_count": 0,
            "fp_count": 0,
        }
    from ultralytics import YOLO

    model = YOLO(str(weights))
    fp = 0
    for img in neg_images:
        res = model.predict(source=str(img), conf=conf, imgsz=imgsz, device=device, verbose=False)
        for r in res:
            if r.boxes is not None and len(r.boxes) > 0:
                fp += 1
    rate = fp / len(neg_images)
    return {
        "fp_on_negatives": rate,
        "fp_on_negatives_note": f"{fp}/{len(neg_images)} images with ≥1 det @conf={conf}",
        "negatives_count": len(neg_images),
        "fp_count": fp,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--weights", type=Path, required=True)
    ap.add_argument("--data-yaml", type=Path, default=CONFIGS / "data_train.yaml")
    ap.add_argument("--raw", type=Path, default=RAW)
    ap.add_argument("--manifests", type=Path, default=MANIFESTS)
    ap.add_argument("--smoke", action="store_true")
    ap.add_argument("--split", type=str, default="val", choices=["val", "test"])
    ap.add_argument("--imgsz", type=int, default=640)
    ap.add_argument("--batch", type=int, default=4)
    ap.add_argument("--device", type=str, default="cpu")
    ap.add_argument("--conf", type=float, default=0.001, help="conf for mAP val (ultralytics default 0.001)")
    ap.add_argument("--fp-conf", type=float, default=0.25, help="conf for FP-on-negatives (deploy-like)")
    ap.add_argument("--out", type=Path, default=None, help="eval_summary.json path")
    ap.add_argument("--name", type=str, default=None, help="run name under runs/")
    args = ap.parse_args()

    manifests = SMOKE if args.smoke else args.manifests
    if not args.weights.is_file():
        print(f"missing weights: {args.weights}", file=sys.stderr)
        return 2

    try:
        from ultralytics import YOLO
    except ImportError:
        print("ERROR: ultralytics missing", file=sys.stderr)
        return 3

    model = YOLO(str(args.weights))
    metrics = model.val(
        data=str(args.data_yaml),
        split=args.split if args.split != "val" else "val",
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device,
        conf=args.conf,
    )

    box = metrics.box
    map50 = float(box.map50) if box.map50 is not None else None
    map50_95 = float(box.map) if box.map is not None else None
    per_class = {}
    if hasattr(box, "ap50") and box.ap50 is not None:
        for i, name in CLASS_NAMES.items():
            try:
                per_class[name] = {
                    "ap50": float(box.ap50[i]) if i < len(box.ap50) else None,
                    "ap": float(box.ap[i]) if hasattr(box, "ap") and i < len(box.ap) else None,
                }
            except Exception:
                per_class[name] = {"ap50": None, "ap": None}
    else:
        for name in CLASS_NAMES.values():
            per_class[name] = {"ap50": None, "ap": None}

    neg_images = count_negatives(args.raw, manifests, split=args.split)
    # Prefer all negatives from meta regardless of split for FP audit
    all_negs = count_negatives(args.raw, manifests, split="train")
    # recount properly: all negative class images
    meta = args.raw / "meta.jsonl"
    neg_all: list[Path] = []
    if meta.is_file():
        for line in meta.read_text().splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("class") == "negative":
                p = args.raw / row["image"]
                if p.is_file():
                    neg_all.append(p)
    fp_info = fp_rate_on_negatives(
        args.weights, neg_all, conf=args.fp_conf, imgsz=args.imgsz, device=args.device
    )

    summary = {
        "weights": str(args.weights),
        "split": args.split,
        "map50": map50,
        "map50_95": map50_95,
        "per_class": per_class,
        **fp_info,
        "imgsz": args.imgsz,
        "conf": args.conf,
        "fp_conf": args.fp_conf,
        "device": args.device,
    }
    print(json.dumps(summary, indent=2))

    out = args.out
    if out is None and args.name:
        out = RUNS / args.name / "eval_summary.json"
    if out is None:
        out = args.weights.parent.parent / "eval_summary.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
