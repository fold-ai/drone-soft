#!/usr/bin/env python3
"""EO↔IMU extrinsics stub — validates lever-arm YAML schema.

Navigation owns the calibration process; System stack hosts this path.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED_TOP = {
    "version",
    "imu_frame",
    "eo_frame",
    "lever_arm_imu_m",
    "rpy_imu_to_eo_rad",
    "t_offset_s",
}


def load_yaml_lite(path: Path) -> dict:
    """Minimal YAML subset loader (no PyYAML required for example files)."""
    try:
        import yaml  # type: ignore

        with path.open() as f:
            return yaml.safe_load(f)
    except ImportError:
        # Fallback: parse our example-style flat keys only via json if .json
        if path.suffix == ".json":
            import json

            return json.loads(path.read_text())
        # Very small subset: key: value and nested x/y/z under known blocks
        data: dict = {}
        stack: list = [data]
        indents = [-1]
        current_map = data
        pending_key = None
        for raw in path.read_text().splitlines():
            if not raw.strip() or raw.strip().startswith("#"):
                continue
            indent = len(raw) - len(raw.lstrip(" "))
            line = raw.strip()
            if line.startswith("notes:"):
                data["notes"] = ""
                continue
            if ":" not in line:
                continue
            key, _, val = line.partition(":")
            key = key.strip()
            val = val.strip().strip("'\"")
            while indents and indent <= indents[-1]:
                stack.pop()
                indents.pop()
            current_map = stack[-1]
            if val == "":
                nested: dict = {}
                current_map[key] = nested
                stack.append(nested)
                indents.append(indent)
            else:
                if val.replace(".", "", 1).replace("-", "", 1).isdigit():
                    current_map[key] = float(val) if "." in val else int(val)
                else:
                    current_map[key] = val
        return data


def validate(doc: dict) -> list[str]:
    errs = []
    missing = REQUIRED_TOP - set(doc)
    if missing:
        errs.append(f"missing keys: {sorted(missing)}")
    la = doc.get("lever_arm_imu_m") or {}
    for k in ("x", "y", "z"):
        if k not in la:
            errs.append(f"lever_arm_imu_m.{k} missing")
    rpy = doc.get("rpy_imu_to_eo_rad") or {}
    for k in ("roll", "pitch", "yaw"):
        if k not in rpy:
            errs.append(f"rpy_imu_to_eo_rad.{k} missing")
    return errs


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "yaml_path",
        nargs="?",
        type=Path,
        default=Path(__file__).with_name("lever_arm.example.yaml"),
    )
    args = ap.parse_args(argv)
    doc = load_yaml_lite(args.yaml_path)
    errs = validate(doc)
    if errs:
        print(f"FAIL {args.yaml_path}")
        for e in errs:
            print(f"  - {e}")
        return 2
    la = doc["lever_arm_imu_m"]
    print(
        f"PASS {args.yaml_path}: lever_arm=({la['x']},{la['y']},{la['z']}) m "
        f"t_offset={doc['t_offset_s']} s"
    )
    print("Note: Navigation owns estimation; this tool only validates schema.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
