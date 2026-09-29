#!/usr/bin/env python3
"""Run the GCS dev server and Nose EO sidecar as one supervised process group."""
from __future__ import annotations

import signal
import socket
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GCS = ROOT / "gcs-ui"


def port_open(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.25):
            return True
    except OSError:
        return False


def stop_process(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=4)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2)


def main() -> int:
    sidecar: subprocess.Popen[bytes] | None = None
    ui: subprocess.Popen[bytes] | None = None
    stopping = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    python = ROOT / ".venv-nose-eo" / "bin" / "python"
    python_cmd = str(python) if python.exists() else sys.executable

    try:
        if port_open("127.0.0.1", 8765):
            print("Nose EO sidecar already listening on 127.0.0.1:8765; reusing it.", flush=True)
        else:
            sidecar = subprocess.Popen(
                [
                    python_cmd,
                    str(ROOT / "tools" / "nose_eo_demo_server.py"),
                    "--no-camera",
                    "--port",
                    "8765",
                ],
                cwd=ROOT,
            )

        ui = subprocess.Popen(
            ["npm", "run", "dev:ui", "--", *sys.argv[1:]],
            cwd=GCS,
        )

        while not stopping:
            if ui.poll() is not None:
                return int(ui.returncode or 0)
            if sidecar is not None and sidecar.poll() is not None:
                print(
                    f"Nose EO sidecar stopped unexpectedly (exit {sidecar.returncode}); stopping UI.",
                    file=sys.stderr,
                    flush=True,
                )
                return int(sidecar.returncode or 1)
            time.sleep(0.25)
        return 0
    finally:
        stop_process(ui)
        stop_process(sidecar)


if __name__ == "__main__":
    raise SystemExit(main())
