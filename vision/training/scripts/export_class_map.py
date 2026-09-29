#!/usr/bin/env python3
"""Export vision/configs/classes_range_v1.yaml → training/exports/class_map_range_v1.json."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]  # vision/
YAML = ROOT / "configs" / "classes_range_v1.yaml"
OUT = ROOT / "training" / "exports" / "class_map_range_v1.json"


def main() -> None:
    names: dict[str, str] = {}
    gate: list[int] = []
    nc = 0
    for line in YAML.read_text().splitlines():
        line = line.strip()
        if line.startswith("nc:"):
            nc = int(line.split(":", 1)[1].strip())
        elif line.startswith("gate:"):
            raw = line.split(":", 1)[1].strip().strip("[]")
            gate = [int(x.strip()) for x in raw.split(",") if x.strip()]
        elif line[:1].isdigit() and ":" in line:
            # "0: shahed_136" under names
            k, v = line.split(":", 1)
            if k.strip().isdigit():
                names[k.strip()] = v.strip()
    payload = {
        "version": 1,
        "name": "range_v1",
        "description": "V1 range-test Lock class map. Exported for Tracking + detector deploy.",
        "nc": nc or len(names),
        "names": names,
        "gate": gate or [int(k) for k in names],
        "legacy_not_in_gate": {"10": "quad", "11": "fixed_wing"},
        "source_yaml": "vision/configs/classes_range_v1.yaml",
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
