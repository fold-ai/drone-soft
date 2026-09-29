#!/usr/bin/env python3
"""Export common Ultralytics training outputs to the training metrics schema."""
from __future__ import annotations

import argparse
import csv
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
TRAINING_DIR = SCRIPT_DIR.parent
REPO_DIR = TRAINING_DIR.parent.parent
CLASS_NAMES = ("shahed_136", "geran_2", "gerbera")


def parse_scalar(value: str) -> Any:
    """Parse the small scalar subset needed from args.yaml without PyYAML."""
    value = value.strip()
    if not value or value in {"null", "Null", "NULL", "~"}:
        return None
    if (value.startswith("'") and value.endswith("'")) or (
        value.startswith('"') and value.endswith('"')
    ):
        return value[1:-1]
    if value.lower() in {"true", "false"}:
        return value.lower() == "true"
    try:
        return int(value)
    except ValueError:
        try:
            return float(value)
        except ValueError:
            return value


def read_args_yaml(path: Path) -> dict[str, Any]:
    """Read top-level key/value pairs; missing PyYAML is not an error."""
    if not path.is_file():
        return {}
    values: dict[str, Any] = {}
    try:
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.lstrip().startswith("#") or ":" not in line:
                continue
            key, raw = line.split(":", 1)
            if key[:1].isspace() or key.strip() != key:
                continue
            values[key.strip()] = parse_scalar(raw.split(" #", 1)[0])
    except OSError:
        return {}
    return values


def number(value: Any) -> float | None:
    try:
        if value is None or str(value).strip() == "":
            return None
        return float(str(value).strip())
    except (TypeError, ValueError):
        return None


def metric_value(row: dict[str, Any], candidates: tuple[str, ...]) -> float | None:
    normalized = {str(k).strip().lower(): v for k, v in row.items()}
    for candidate in candidates:
        result = number(normalized.get(candidate.lower()))
        if result is not None:
            return result
    return None


def per_class_values(row: dict[str, Any]) -> dict[str, dict[str, float] | None]:
    """Use per-class columns when a result exporter provides them."""
    result: dict[str, dict[str, float] | None] = {}
    for class_name in CLASS_NAMES:
        value: float | None = None
        for key, raw in row.items():
            lowered = str(key).lower()
            if class_name in lowered and ("map50" in lowered or "ap50" in lowered):
                value = number(raw)
                if value is not None:
                    break
        result[class_name] = {"ap50": value} if value is not None else None
    return result


def read_results(path: Path) -> list[tuple[str, dict[str, Any]]]:
    if not path.is_file():
        return []
    try:
        with path.open(newline="", encoding="utf-8-sig") as handle:
            rows = [dict(row) for row in csv.DictReader(handle) if any(row.values())]
    except (OSError, csv.Error):
        return []
    if not rows:
        return []

    # Ultralytics results.csv is aggregate validation output. If another exporter
    # includes a split column, preserve its distinct splits.
    split_rows: dict[str, dict[str, Any]] = {}
    for row in rows:
        split = str(row.get("split") or row.get("Split") or "val").strip().lower() or "val"
        split_rows[split] = row
    return [(split, row) for split, row in split_rows.items()]


def iso_from_args(values: dict[str, Any], run_dir: Path) -> str:
    for key in ("started_at", "start_time", "created_at", "date"):
        raw = values.get(key)
        if raw:
            return str(raw)
    try:
        timestamp = run_dir.stat().st_mtime
    except OSError:
        timestamp = datetime.now(timezone.utc).timestamp()
    return datetime.fromtimestamp(timestamp, timezone.utc).isoformat().replace("+00:00", "Z")


def git_sha() -> str | None:
    try:
        completed = subprocess.run(
            ["git", "-C", str(REPO_DIR), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    value = completed.stdout.strip()
    return value or None


def display_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_DIR.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def artifacts_for(run_dir: Path, run_id: str) -> list[dict[str, Any]]:
    artifacts: list[dict[str, Any]] = []
    seen: set[Path] = set()

    def add(kind: str, path: Path) -> None:
        resolved = path.resolve()
        if resolved in seen or not path.is_file():
            return
        seen.add(resolved)
        artifacts.append({"run_id": run_id, "kind": kind, "storage_path": display_path(path)})

    add("weights", run_dir / "weights" / "best.pt")
    add("weights", run_dir / "weights" / "last.pt")
    if run_dir.is_dir():
        for path in sorted(run_dir.rglob("*.onnx")):
            add("onnx", path)
        add("log", run_dir / "results.csv")
        for path in sorted(run_dir.rglob("*.log")):
            add("log", path)
    return artifacts


def build_export(
    run_dir: Path,
    model: str | None,
    epochs: int | None,
    notes: str | None,
    sha: str | None,
) -> dict[str, Any]:
    run_id = run_dir.name or "training-run"
    args = read_args_yaml(run_dir / "args.yaml")
    result_rows = read_results(run_dir / "results.csv")

    resolved_model = model if model is not None else args.get("model")
    resolved_epochs: Any = epochs if epochs is not None else args.get("epochs")
    if resolved_epochs is None and result_rows:
        raw_epoch = result_rows[-1][1].get("epoch")
        try:
            resolved_epochs = int(float(raw_epoch)) + 1
        except (TypeError, ValueError):
            pass
    resolved_notes = notes if notes is not None else args.get("notes")
    resolved_sha = sha if sha is not None else git_sha()

    metrics: list[dict[str, Any]] = []
    rows = result_rows or [("val", {})]
    for split, row in rows:
        metrics.append(
            {
                "run_id": run_id,
                "split": split,
                "map50": metric_value(
                    row,
                    ("metrics/mAP50(B)", "metrics/mAP50", "map50", "mAP50", "map50(b)"),
                ),
                "map50_95": metric_value(
                    row,
                    (
                        "metrics/mAP50-95(B)",
                        "metrics/mAP50-95",
                        "map50_95",
                        "mAP50-95",
                        "map50-95",
                    ),
                ),
                "per_class": per_class_values(row),
            }
        )

    return {
        "schema_version": 1,
        "run": {
            "id": run_id,
            "started_at": iso_from_args(args, run_dir),
            "model": resolved_model,
            "epochs": resolved_epochs,
            "git_sha": resolved_sha,
            "notes": resolved_notes,
        },
        "metrics": metrics,
        "artifacts": artifacts_for(run_dir, run_id),
    }


def demo_export() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "run": {
            "id": "demo-run",
            "started_at": "2026-01-01T00:00:00Z",
            "model": "yolov8n.pt",
            "epochs": 10,
            "git_sha": None,
            "notes": "Sample export generated with --demo.",
        },
        "metrics": [
            {
                "run_id": "demo-run",
                "split": "val",
                "map50": 0.91,
                "map50_95": 0.67,
                "per_class": {
                    "shahed_136": {"ap50": 0.93},
                    "geran_2": {"ap50": 0.89},
                    "gerbera": {"ap50": 0.91},
                },
            }
        ],
        "artifacts": [
            {
                "run_id": "demo-run",
                "kind": "weights",
                "storage_path": "vision/training/runs/demo-run/weights/best.pt",
            }
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-dir", type=Path, help="Training run directory (or any arbitrary path).")
    parser.add_argument(
        "--out",
        type=Path,
        help="Output JSON path (default: vision/training/exports/metrics_<run_id>.json).",
    )
    parser.add_argument("--model", help="Override the model name/path.")
    parser.add_argument("--epochs", type=int, help="Override the epoch count.")
    parser.add_argument("--notes", help="Notes to include in the run record.")
    parser.add_argument("--git-sha", help="Override the Git SHA (default: git rev-parse HEAD).")
    parser.add_argument("--demo", action="store_true", help="Write a valid sample without a real run.")
    args = parser.parse_args()

    if args.demo:
        payload = demo_export()
        default_out = TRAINING_DIR / "exports" / "metrics_demo.json"
    else:
        if args.run_dir is None:
            parser.error("--run-dir is required unless --demo is used")
        run_dir = args.run_dir.expanduser()
        payload = build_export(run_dir, args.model, args.epochs, args.notes, args.git_sha)
        default_out = TRAINING_DIR / "exports" / f"metrics_{payload['run']['id']}.json"

    out = args.out or default_out
    out = out.expanduser()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
