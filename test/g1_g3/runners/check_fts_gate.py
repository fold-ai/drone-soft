#!/usr/bin/env python3
"""Block G2/G3 schedule when FTS holder is unnamed."""
from __future__ import annotations

import argparse
import sys


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("gate", choices=["G1", "G2", "G3"])
    p.add_argument("--fts-holder", default="", help="Named holder; empty = unnamed")
    args = p.parse_args()

    holder = (args.fts_holder or "").strip()
    if args.gate == "G1":
        print("G1: FTS holder not required")
        return 0
    if not holder:
        print(
            f"FAIL: {args.gate} blocked — FTS holder unnamed "
            "(V1_PLAN B8; no green path until named)",
            file=sys.stderr,
        )
        return 2
    print(f"OK: {args.gate} FTS holder = {holder}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
