#!/usr/bin/env python3
"""Verify the local, observation-only Nose EO demo stack.

The check intentionally does not connect to vehicle telemetry, send MAVLink,
initialize a visual track, or issue mission commands.  It only verifies that
the local UI is reachable, browser frames reach the demo detector, the payload
remains explicitly non-lock-eligible, and the host tracking-policy tests pass.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Result:
    name: str
    status: str
    detail: str


def fetch_json(url: str, timeout: float) -> dict[str, Any]:
    request = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        payload = json.loads(response.read().decode("utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("response is not a JSON object")
    return payload


def check_ui(url: str, timeout: float) -> Result:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as response:
            body = response.read(256_000).decode("utf-8", errors="replace")
            if response.status != 200:
                return Result("UI", "FAIL", f"HTTP {response.status}")
            if "ACTPROVE" not in body and 'id="root"' not in body:
                return Result("UI", "FAIL", "unexpected page body")
        return Result("UI", "PASS", url)
    except (OSError, urllib.error.URLError) as exc:
        return Result("UI", "FAIL", str(exc))


def check_health(base_url: str, timeout: float) -> Result:
    try:
        payload = fetch_json(f"{base_url}/health", timeout)
    except (OSError, ValueError, json.JSONDecodeError, urllib.error.URLError) as exc:
        return Result("Detector health", "FAIL", str(exc))

    if payload.get("ok") is not True:
        return Result("Detector health", "FAIL", str(payload.get("error") or "ok=false"))
    if payload.get("demo") is not True:
        return Result("Detector health", "FAIL", "endpoint is not marked demo=true")
    detail = f"{payload.get('detector', 'unknown')} · {payload.get('camera_label', 'no camera label')}"
    return Result("Detector health", "PASS", detail)


def check_live_frame(base_url: str, timeout: float, require_live_frame: bool) -> Result:
    try:
        payload = fetch_json(f"{base_url}/detect", timeout)
    except (OSError, ValueError, json.JSONDecodeError, urllib.error.URLError) as exc:
        return Result("Browser frame", "FAIL", str(exc))

    if payload.get("lock_eligible") is not False:
        return Result("Browser frame", "FAIL", "demo payload must remain lock_eligible=false")
    width = int(payload.get("frame_w") or 0)
    height = int(payload.get("frame_h") or 0)
    fps = float(payload.get("fps") or 0.0)
    camera_label = str(payload.get("camera_label") or "")
    has_live_frame = bool(payload.get("ok") and width > 0 and height > 0 and fps > 0)
    if not has_live_frame:
        status = "FAIL" if require_live_frame else "WARN"
        return Result("Browser frame", status, "no live browser frame received")
    if not camera_label.startswith("browser"):
        return Result("Browser frame", "FAIL", f"unexpected source: {camera_label or 'none'}")
    return Result("Browser frame", "PASS", f"{width}x{height} · {fps:.1f} FPS · {camera_label}")


def check_negative_dataset(base_url: str, timeout: float) -> Result:
    try:
        payload = fetch_json(f"{base_url}/dataset/negative/status", timeout)
    except (OSError, ValueError, json.JSONDecodeError, urllib.error.URLError) as exc:
        return Result("Negative dataset", "FAIL", str(exc))

    count = int(payload.get("count") or 0)
    minimum = int(payload.get("minimum") or 0)
    if payload.get("ready") is True:
        return Result("Negative dataset", "PASS", f"{count}/{minimum}")
    return Result("Negative dataset", "WARN", f"{count}/{minimum}; more operator-captured negatives required")


def check_host_tests(timeout: float) -> Result:
    command = [sys.executable, str(ROOT / "tools" / "test_nose_eo_tracking.py")]
    try:
        completed = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return Result("Host policy tests", "FAIL", str(exc))
    if completed.returncode != 0 or "OVERALL PASS" not in completed.stdout:
        detail = (completed.stderr or completed.stdout or "test failed").strip().splitlines()[-1]
        return Result("Host policy tests", "FAIL", detail)
    passed = sum(line.strip().startswith("PASS  ") for line in completed.stdout.splitlines())
    return Result("Host policy tests", "PASS", f"{passed} checks")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ui-url", default="http://127.0.0.1:5173/")
    parser.add_argument("--detector-url", default="http://127.0.0.1:8765")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument(
        "--allow-no-live-frame",
        action="store_true",
        help="warn instead of fail when the browser has not submitted a camera frame",
    )
    args = parser.parse_args()

    base_url = args.detector_url.rstrip("/")
    results = [
        check_ui(args.ui_url, args.timeout),
        check_health(base_url, args.timeout),
        check_live_frame(base_url, args.timeout, not args.allow_no_live_frame),
        check_negative_dataset(base_url, args.timeout),
        check_host_tests(max(args.timeout, 30.0)),
    ]

    for result in results:
        print(f"{result.status:4}  {result.name}: {result.detail}")

    failed = sum(result.status == "FAIL" for result in results)
    warned = sum(result.status == "WARN" for result in results)
    print(f"SUMMARY: {len(results) - failed - warned} PASS, {warned} WARN, {failed} FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
