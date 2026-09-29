#!/usr/bin/env python3
"""Build and attest a TensorRT engine on the target Jetson.

The wrapper is fail-closed: Linux/aarch64, ONNX input, and trtexec are required.
It never downloads a model and never accesses cameras or flight-control devices.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--onnx", type=Path, required=True)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--input-name", default=None)
    parser.add_argument("--shape", default=None, help="example: 1x3x640x640")
    parser.add_argument("--workspace-mib", type=int, default=1024)
    parser.add_argument("--trtexec", type=Path, default=None)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--meta", type=Path, default=None)
    args = parser.parse_args()

    machine = platform.machine().lower()
    if platform.system() != "Linux" or machine not in {"aarch64", "arm64"}:
        print(f"REFUSED: TensorRT engine must be built on Linux/aarch64; found {platform.system()}/{machine}")
        return 2
    if not args.onnx.is_file() or args.onnx.stat().st_size < 1024:
        print(f"REFUSED: missing or implausibly small ONNX file: {args.onnx}")
        return 3
    if args.engine.exists() and not args.force:
        print(f"REFUSED: engine already exists; use --force to replace: {args.engine}")
        return 4
    if bool(args.input_name) != bool(args.shape):
        print("REFUSED: --input-name and --shape must be supplied together")
        return 5
    if args.workspace_mib < 128:
        print("REFUSED: --workspace-mib must be at least 128")
        return 6

    candidates = [
        args.trtexec,
        Path("/usr/src/tensorrt/bin/trtexec"),
        Path(shutil.which("trtexec")) if shutil.which("trtexec") else None,
    ]
    trtexec = next((path for path in candidates if path and path.is_file()), None)
    if trtexec is None:
        print("REFUSED: trtexec not found")
        return 7

    args.engine.parent.mkdir(parents=True, exist_ok=True)
    meta_path = args.meta or args.engine.with_suffix(args.engine.suffix + ".json")
    log_path = args.engine.with_suffix(args.engine.suffix + ".build.log")
    command = [
        str(trtexec),
        f"--onnx={args.onnx.resolve()}",
        f"--saveEngine={args.engine.resolve()}",
        "--fp16",
        f"--memPoolSize=workspace:{args.workspace_mib}MiB",
    ]
    if args.input_name and args.shape:
        command.append(f"--shapes={args.input_name}:{args.shape}")

    version = subprocess.run(
        [str(trtexec), "--version"], capture_output=True, text=True, check=False
    )
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
    if completed.returncode != 0 or not args.engine.is_file() or args.engine.stat().st_size < 1024:
        print(f"FAILED: trtexec exit={completed.returncode}; inspect {log_path}")
        return 8

    payload = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "host": {"system": platform.system(), "machine": machine, "release": platform.release()},
        "trtexec": str(trtexec),
        "trtexec_version": (version.stdout + version.stderr).strip(),
        "precision": "fp16",
        "onnx": {"path": str(args.onnx.resolve()), "sha256": sha256(args.onnx)},
        "engine": {"path": str(args.engine.resolve()), "sha256": sha256(args.engine)},
        "shape": (
            {"input": args.input_name, "value": args.shape}
            if args.input_name and args.shape
            else "network_default"
        ),
        "flight_control_access": False,
        "validation_status": "BUILT_NOT_BENCHMARKED",
    }
    meta_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2))
    print(f"wrote {args.engine}, {meta_path}, and {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
