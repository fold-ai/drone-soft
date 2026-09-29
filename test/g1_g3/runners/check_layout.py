#!/usr/bin/env python3
"""Validate the minimum gate archive layout without writing PASS/FAIL markers."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

REQUIRED_MANIFEST = {
    "gate", "run_id", "started_at", "operator", "git_sha", "fts_holder",
    "airspeed_source",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    root = args.archive
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        raise SystemExit("manifest.json missing")
    try:
        manifest = json.loads(manifest_path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"invalid manifest.json: {exc}") from exc
    missing = sorted(REQUIRED_MANIFEST - manifest.keys())
    if missing:
        raise SystemExit(f"manifest fields missing: {', '.join(missing)}")
    if manifest["gate"] not in {"G1", "G2", "G3"}:
        raise SystemExit(f"invalid gate: {manifest['gate']!r}")
    if not (root / "streams").is_dir():
        raise SystemExit("streams/ missing")
    print("layout ok", root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
