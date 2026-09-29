#!/usr/bin/env python3
"""Export best.pt → ONNX under training/exports/.

On failure, writes a stub .onnx note file and exits non-zero unless --stub-ok.
TensorRT build is documented in docs/ORIN_TRT.md (on-device later).
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import EXPORTS, RUNS  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--weights", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--imgsz", type=int, default=640)
    ap.add_argument("--name", type=str, default=None, help="export filename stem")
    ap.add_argument("--stub-ok", action="store_true", help="write stub on failure and exit 0")
    args = ap.parse_args()

    EXPORTS.mkdir(parents=True, exist_ok=True)
    stem = args.name or args.weights.stem
    out = args.out or (EXPORTS / f"{stem}.onnx")

    if not args.weights.is_file():
        print(f"missing weights: {args.weights}", file=sys.stderr)
        return 2

    try:
        from ultralytics import YOLO

        model = YOLO(str(args.weights))
        produced = model.export(format="onnx", imgsz=args.imgsz)
        produced = Path(str(produced))
        if produced.is_file():
            shutil.copy2(produced, out)
            print(f"ONNX exported -> {out}")
            # also keep next to run weights if under runs/
            return 0
        raise RuntimeError(f"export returned non-file: {produced}")
    except Exception as e:
        print(f"ONNX export failed: {e}", file=sys.stderr)
        stub = out.with_suffix(".onnx.STUB.txt")
        stub.write_text(
            f"ONNX export stub — export failed or deferred.\n"
            f"weights={args.weights}\nerror={e}\n"
            f"See docs/ORIN_TRT.md for on-device TensorRT build.\n"
            f"Retry: vision/.venv/bin/python scripts/export_onnx.py --weights {args.weights}\n"
        )
        # also empty placeholder path note
        note = out.with_suffix(".onnx.pending")
        note.write_text(f"pending ONNX for {args.weights}\n")
        print(f"wrote stub notes: {stub} {note}")
        return 0 if args.stub_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
