#!/usr/bin/env python3
"""Write / merge metrics.json for a training run (local; Supabase upload optional no-op).

Schema aligned with docs/TRAINING_METRICS_SUPABASE.md tables:
  training_runs, training_metrics, training_artifacts
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _paths import EXPORTS, RUNS  # noqa: E402


def optional_supabase_upload(payload: dict) -> dict:
    """No-op without SUPABASE_URL + SUPABASE_SERVICE_KEY (or ANON). Never commits secrets."""
    url = os.environ.get("SUPABASE_URL") or os.environ.get("TRAINING_SUPABASE_URL")
    key = (
        os.environ.get("SUPABASE_SERVICE_KEY")
        or os.environ.get("SUPABASE_ANON_KEY")
        or os.environ.get("TRAINING_SUPABASE_KEY")
    )
    if not url or not key:
        return {"uploaded": False, "reason": "no Supabase env keys — local only"}
    # Stub: do not actually call without explicit --upload flag handled by caller
    return {"uploaded": False, "reason": "keys present but upload not implemented in local stub"}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--run-dir", type=Path, required=True, help="training/runs/<name>/")
    ap.add_argument("--eval-summary", type=Path, default=None)
    ap.add_argument("--run-id", type=str, default=None)
    ap.add_argument("--model", type=str, default="yolov8n")
    ap.add_argument("--epochs", type=int, default=None)
    ap.add_argument("--split", type=str, default="val")
    ap.add_argument("--notes", type=str, default="")
    ap.add_argument("--onnx", type=Path, default=None)
    ap.add_argument("--upload", action="store_true", help="attempt upload (no-ops without keys)")
    args = ap.parse_args()

    run_dir = args.run_dir
    run_dir.mkdir(parents=True, exist_ok=True)
    best = run_dir / "weights" / "best.pt"
    last = run_dir / "weights" / "last.pt"
    eval_path = args.eval_summary or (run_dir / "eval_summary.json")
    eval_data = {}
    if eval_path.is_file():
        eval_data = json.loads(eval_path.read_text())

    # parse TRAIN_NOTE for epochs if needed
    epochs = args.epochs
    note = run_dir / "TRAIN_NOTE.txt"
    if epochs is None and note.is_file():
        for line in note.read_text().splitlines():
            if line.startswith("epochs="):
                try:
                    epochs = int(line.split("=", 1)[1])
                except ValueError:
                    pass

    started_at = datetime.now(timezone.utc).isoformat()
    # prefer results.csv mtime if present
    results_csv = run_dir / "results.csv"
    if results_csv.is_file():
        started_at = datetime.fromtimestamp(results_csv.stat().st_mtime, tz=timezone.utc).isoformat()

    run_id = args.run_id or f"{run_dir.name}_{uuid.uuid4().hex[:8]}"
    onnx_path = args.onnx
    if onnx_path is None:
        cand = list(EXPORTS.glob(f"{run_dir.name}*.onnx"))
        onnx_path = cand[0] if cand else None

    per_class = eval_data.get("per_class") or {}
    payload = {
        "run_id": run_id,
        "started_at": started_at,
        "model": args.model,
        "epochs": epochs,
        "git_sha": None,
        "notes": args.notes
        or (
            "smoke provisional labels"
            if "smoke" in run_dir.name
            else ""
        ),
        "split": args.split,
        "map50": eval_data.get("map50"),
        "map50_95": eval_data.get("map50_95"),
        "per_class": per_class,
        "fp_on_negatives": eval_data.get("fp_on_negatives"),
        "fp_on_negatives_note": eval_data.get("fp_on_negatives_note"),
        "negatives_count": eval_data.get("negatives_count", 0),
        "artifacts": {
            "weights_best": str(best) if best.is_file() else None,
            "weights_last": str(last) if last.is_file() else None,
            "onnx": str(onnx_path) if onnx_path and Path(onnx_path).is_file() else None,
            "eval_summary": str(eval_path) if eval_path.is_file() else None,
            "results_csv": str(results_csv) if results_csv.is_file() else None,
        },
        # table-shaped blocks for future upload
        "training_runs": {
            "id": run_id,
            "started_at": started_at,
            "model": args.model,
            "epochs": epochs,
            "git_sha": None,
            "notes": args.notes,
        },
        "training_metrics": {
            "run_id": run_id,
            "split": args.split,
            "map50": eval_data.get("map50"),
            "map50_95": eval_data.get("map50_95"),
            "per_class": per_class,
            "fp_on_negatives": eval_data.get("fp_on_negatives"),
        },
        "training_artifacts": [
            {"run_id": run_id, "kind": "weights", "storage_path": str(best)}
            for _ in [0]
            if best.is_file()
        ]
        + (
            [{"run_id": run_id, "kind": "onnx", "storage_path": str(onnx_path)}]
            if onnx_path and Path(onnx_path).is_file()
            else []
        )
        + (
            [{"run_id": run_id, "kind": "log", "storage_path": str(results_csv)}]
            if results_csv.is_file()
            else []
        ),
    }

    out = run_dir / "metrics.json"
    out.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"wrote {out}")

    if args.upload:
        status = optional_supabase_upload(payload)
        print(json.dumps({"supabase": status}))
    else:
        print(json.dumps({"supabase": optional_supabase_upload(payload)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
