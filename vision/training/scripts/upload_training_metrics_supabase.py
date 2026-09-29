#!/usr/bin/env python3
"""Plan or execute insertion of an exported training metrics JSON document.

The Supabase service-role key is server-side only. Never ship it in a browser or
Next.js client; use a protected server-side job or dashboard API instead.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any


def rows_from_payload(payload: dict[str, Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    run = payload.get("run")
    metrics = payload.get("metrics")
    artifacts = payload.get("artifacts")
    if not isinstance(run, dict) or not isinstance(metrics, list) or not isinstance(artifacts, list):
        raise ValueError("metrics JSON must contain run, metrics, and artifacts")
    return [run], metrics, artifacts


def print_plan(runs: list[dict[str, Any]], metrics: list[dict[str, Any]], artifacts: list[dict[str, Any]]) -> None:
    print("Dry run: no Supabase request will be made.")
    print(f"Would insert {len(runs)} row(s) into training_runs")
    print(f"Would insert {len(metrics)} row(s) into training_metrics")
    print(f"Would insert {len(artifacts)} row(s) into training_artifacts")
    for table, rows in (("training_runs", runs), ("training_metrics", metrics), ("training_artifacts", artifacts)):
        for row in rows:
            print(f"  {table}: {json.dumps(row, sort_keys=True)}")


def execute(
    url: str,
    key: str,
    runs: list[dict[str, Any]],
    metrics: list[dict[str, Any]],
    artifacts: list[dict[str, Any]],
) -> None:
    try:
        from supabase import create_client  # type: ignore
    except ImportError:
        print("Supabase package is not installed. Install it with: python3 -m pip install supabase", file=sys.stderr)
        raise SystemExit(2)

    client = create_client(url, key)
    for table, rows in (("training_runs", runs), ("training_metrics", metrics), ("training_artifacts", artifacts)):
        if not rows:
            continue
        client.table(table).insert(rows).execute()
        print(f"Inserted {len(rows)} row(s) into {table}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="A service_role key must never ship in a browser or Next.js client.",
    )
    parser.add_argument("metrics_json", nargs="?", type=Path, help="Path to exported metrics JSON.")
    parser.add_argument("--metrics", dest="metrics_option", type=Path, help="Alias for metrics_json.")
    parser.add_argument("--dry-run", action="store_true", help="Print planned inserts (the default).")
    parser.add_argument("--execute", action="store_true", help="Insert rows using the Supabase Python client.")
    args = parser.parse_args()

    metrics_path = args.metrics_option or args.metrics_json
    if metrics_path is None:
        parser.error("a metrics JSON path is required (unless only requesting --help)")
    if args.dry_run and args.execute:
        parser.error("choose only one of --dry-run and --execute")

    try:
        payload = json.loads(metrics_path.read_text(encoding="utf-8"))
        runs, metrics, artifacts = rows_from_payload(payload)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"Cannot read metrics JSON: {exc}", file=sys.stderr)
        return 2

    if not args.execute:
        print_plan(runs, metrics, artifacts)
        return 0

    url = os.environ.get("SUPABASE_URL")
    key = os.environ.get("SUPABASE_SERVICE_ROLE_KEY") or os.environ.get("SUPABASE_ANON_KEY")
    if not url or not key:
        print(
            "Missing Supabase credentials. Set SUPABASE_URL and SUPABASE_SERVICE_ROLE_KEY "
            "(or SUPABASE_ANON_KEY) for --execute.",
            file=sys.stderr,
        )
        return 2
    try:
        execute(url, key, runs, metrics, artifacts)
    except SystemExit:
        raise
    except Exception as exc:  # keep the optional stub from exposing a stack trace
        print(f"Supabase insert failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
