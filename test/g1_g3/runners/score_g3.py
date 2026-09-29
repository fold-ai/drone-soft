#!/usr/bin/env python3
"""Score G3 campaign: 10 valid sorties inside envelope; else RED."""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

REQUIRED = 10
OWN_MAX = 200.0
OWN_MIN = 150.0
LOCK_MIN, LOCK_MAX = 8.0, 15.0
MISS_MIN, MISS_MAX = 15.0, 30.0
TGT_NOM = 100.0
TGT_MIN, TGT_MAX = 80.0, 120.0


def number(value: object, default: float = math.nan) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text().splitlines() if l.strip()]


def sortie_valid(s: dict) -> tuple[bool, str | None]:
    if s.get("hit_profile"):
        return False, "hit_profile"
    lock = number(s.get("lock_s"), -1)
    if not math.isfinite(lock):
        return False, "lock_s not finite"
    if not (LOCK_MIN <= lock <= LOCK_MAX):
        return False, f"lock_s {lock} outside {LOCK_MIN}-{LOCK_MAX}"
    miss = number(s.get("commanded_miss_m"), -1)
    if not math.isfinite(miss):
        return False, "commanded_miss_m not finite"
    if not (MISS_MIN <= miss <= MISS_MAX):
        return False, f"miss {miss} outside {MISS_MIN}-{MISS_MAX}"
    own = number(s.get("ownship_kmh_max"), 1e9)
    if not math.isfinite(own) or not (OWN_MIN <= own <= OWN_MAX):
        return False, f"ownship {own} outside {OWN_MIN}-{OWN_MAX}"
    target = number(s.get("target_kmh"))
    if not math.isfinite(target) or not (TGT_MIN <= target <= TGT_MAX):
        return False, f"target {target} outside {TGT_MIN}-{TGT_MAX}"
    return True, None


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path)
    args = p.parse_args()
    root = args.archive
    reasons: list[str] = []

    manifest = json.loads((root / "manifest.json").read_text()) if (root / "manifest.json").exists() else {}
    if manifest.get("gate") != "G3":
        reasons.append("manifest gate must be G3")
    holder = (manifest.get("fts_holder") or "").strip()
    if not holder:
        reasons.append("FTS holder unnamed — G3 blocked")

    rows = load_jsonl(root / "sorties.jsonl")
    valid = 0
    own_max_obs = 0.0
    for s in rows:
        ok, why = sortie_valid(s)
        own_max_obs = max(own_max_obs, number(s.get("ownship_kmh_max"), 0))
        if ok and s.get("valid") is True:
            valid += 1
        elif ok:
            reasons.append(f"sortie {s.get('sortie_id', '?')}: valid must be explicitly true")
        elif why:
            s_id = s.get("sortie_id", "?")
            reasons.append(f"sortie {s_id}: {why}")

    if own_max_obs > OWN_MAX:
        reasons.append(f"campaign ownship max {own_max_obs} > {OWN_MAX}")

    if valid >= REQUIRED and not reasons:
        result = "PASS"
    else:
        result = "RED"
        if valid < REQUIRED:
            reasons.append(f"valid_sorties {valid} < {REQUIRED}")

    summary = {
        "gate": "G3",
        "result": result,
        "fts_holder": holder or None,
        "valid_sorties": valid,
        "required_sorties": REQUIRED,
        "ownship_kmh_max_observed": own_max_obs,
        "target_kmh_nominal": TGT_NOM,
        "envelope_frozen": result == "RED",
        "fail_reasons": reasons,
        "archive_path": str(root),
    }
    print(json.dumps(summary, indent=2))
    return 0 if result == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
