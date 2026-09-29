#!/usr/bin/env python3
"""Score a G2 archive. Requires named FTS holder + FC heading proof."""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

ALLOWED = {"BOOT", "SEARCH", "LOCK", "CLOSE", "ABORT", "RTB", "FTS"}


def finite_number(value: object) -> bool:
    try:
        return math.isfinite(float(value))
    except (TypeError, ValueError):
        return False


def load_json(path: Path):
    return json.loads(path.read_text()) if path.exists() else None


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text().splitlines() if l.strip()]


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path)
    args = p.parse_args()
    root = args.archive
    reasons: list[str] = []

    manifest = load_json(root / "manifest.json") or {}
    if manifest.get("gate") != "G2":
        reasons.append("manifest gate must be G2")
    holder = (manifest.get("fts_holder") or "").strip()
    if not holder:
        reasons.append("FTS holder unnamed — G2 blocked")

    heading = False
    proof = root / "fc" / "heading_proof.txt"
    if proof.exists() and proof.read_text().strip():
        heading = True
    elif (root / "fc").exists() and any((root / "fc").iterdir()):
        # excerpt present but proof text required for auto-score
        reasons.append("fc/ present but heading_proof.txt empty/missing")
    else:
        reasons.append("FC heading proof missing")

    setpoints = load_jsonl(root / "streams" / "mavlink_setpoints.jsonl")
    valid_setpoints = [
        row for row in setpoints
        if row.get("type_mask") == 0x09C7
        and all(finite_number(row.get(k)) for k in ("vx", "vy", "vz", "yaw"))
    ]
    if not valid_setpoints:
        reasons.append("valid velocity+yaw MAVLink setpoint evidence missing")

    states = load_jsonl(root / "streams" / "lock_state.jsonl")
    states_seen = []
    work_gated = True
    for row in states:
        st = row.get("state")
        if st not in ALLOWED:
            reasons.append(f"illegal state {st!r}")
        else:
            states_seen.append(st)
        if st == "SEARCH" and row.get("prev") in ("BOOT", "ABORT") and row.get("cmd") != "WORK":
            work_gated = False
            reasons.append("SEARCH entered without WORK after BOOT/ABORT")

    if not states:
        reasons.append("lock_state.jsonl missing")

    fts_path = root / "fc" / "fts_assert.json"
    fts = load_json(fts_path) if fts_path.exists() else None
    fts_scripted = fts is not None
    if fts_scripted and not all(
        fts.get(k) is True
        for k in ("throttle_idle", "surfaces_fixed", "fuel_cut_relay")
    ):
        reasons.append("scripted FTS evidence is incomplete")

    result = "PASS" if not reasons and heading and work_gated and holder else "FAIL"
    summary = {
        "gate": "G2",
        "result": result,
        "fts_holder": holder or None,
        "heading_in_fc_log": heading,
        "valid_setpoint_count": len(valid_setpoints),
        "fc_excerpt_path": str(proof) if proof.exists() else None,
        "work_gated_search": work_gated,
        "states_seen": sorted(set(states_seen)),
        "fts_scripted": fts_scripted,
        "fts_idle_asserted": None if not fts_scripted else fts.get("throttle_idle") is True,
        "fts_surfaces_fixed": None if not fts_scripted else fts.get("surfaces_fixed") is True,
        "fts_fuel_cut_asserted": None if not fts_scripted else fts.get("fuel_cut_relay") is True,
        "fail_reasons": reasons,
        "archive_path": str(root),
    }
    print(json.dumps(summary, indent=2))
    return 0 if result == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
