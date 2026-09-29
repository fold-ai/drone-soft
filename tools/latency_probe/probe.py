#!/usr/bin/env python3
"""CLI alias → latency_probe.py."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from latency_probe import main

if __name__ == "__main__":
    sys.exit(main())
