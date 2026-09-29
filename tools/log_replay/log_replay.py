#!/usr/bin/env python3
"""Offline log replay — validate ICD fields present in a black-box run folder.

Does not re-run perception/navigation; checks streams + required keys against
docs/icd + onboard/logging/schema/blackbox_streams.json.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Set

REQUIRED_STREAMS = {
    "meta.json": None,  # file, not jsonl
    "camera_ts.jsonl": {"measurement_epoch_ns", "seq", "cam_id"},
    "tracks.jsonl": {"measurement_epoch_ns", "track_id", "publish_time_ns"},
}

OPTIONAL_STREAMS = {
    "ownship.jsonl": {"measurement_epoch_ns"},
    "mavlink_tx.jsonl": {"publish_time_ns", "yaw"},
    "mission_state.jsonl": {"state"},
    "boxes.jsonl": {"t_pps", "seq"},
}


def _keys_of_first(path: Path) -> Set[str]:
    with path.open() as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            return set(json.loads(line).keys())
    return set()


def validate_run(run_dir: Path) -> List[str]:
    errors: List[str] = []
    if not run_dir.is_dir():
        return [f"not a directory: {run_dir}"]

    meta = run_dir / "meta.json"
    if not meta.is_file():
        errors.append("missing meta.json")
    else:
        try:
            # meta may be multi-line JSON objects
            text = meta.read_text().strip().splitlines()[0]
            json.loads(text)
        except Exception as e:  # noqa: BLE001
            errors.append(f"meta.json parse: {e}")

    for name, req in REQUIRED_STREAMS.items():
        if name == "meta.json":
            continue
        path = run_dir / name
        if not path.is_file():
            errors.append(f"missing required stream: {name}")
            continue
        keys = _keys_of_first(path)
        if not keys:
            errors.append(f"{name}: empty")
            continue
        # Allow t_pps alias for measurement_epoch_ns
        if "measurement_epoch_ns" in req and "measurement_epoch_ns" not in keys:
            if "t_pps" in keys or "measurement_epoch" in keys:
                pass
            else:
                errors.append(f"{name}: missing measurement_epoch_ns (or t_pps)")
        missing = (req - {"measurement_epoch_ns"}) - keys
        if missing:
            errors.append(f"{name}: missing fields {sorted(missing)}")

    for name, req in OPTIONAL_STREAMS.items():
        path = run_dir / name
        if not path.is_file():
            continue
        keys = _keys_of_first(path)
        if req - keys:
            errors.append(f"{name}: incomplete optional fields {sorted(req - keys)}")

    return errors


def main(argv: List[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("run_dir", type=Path, help="Black-box run folder")
    ap.add_argument(
        "--strict",
        action="store_true",
        help="Also require optional streams ownship + mavlink_tx",
    )
    args = ap.parse_args(argv)

    errors = validate_run(args.run_dir)
    if args.strict:
        for name in ("ownship.jsonl", "mavlink_tx.jsonl"):
            if not (args.run_dir / name).is_file():
                errors.append(f"strict: missing {name}")

    if errors:
        print(f"FAIL {args.run_dir}")
        for e in errors:
            print(f"  - {e}")
        return 2
    print(f"PASS {args.run_dir}: ICD fields present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
