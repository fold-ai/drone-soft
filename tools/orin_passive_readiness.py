#!/usr/bin/env python3
"""Fail-closed readiness audit for the passive Orin vision observer.

This tool never opens a camera, serial port, RC device, or network socket. It
only inspects the local filesystem and build host. Passing this audit is not
flight evidence; it means the passive camera/detector service has its required
local artifacts.
"""
from __future__ import annotations

import argparse
import json
import platform
import shutil
import struct
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Check:
    name: str
    status: str
    detail: str


def check(condition: bool, name: str, ok: str, failure: str, *, warning: bool = False) -> Check:
    if condition:
        return Check(name, "PASS", ok)
    return Check(name, "WARN" if warning else "FAIL", failure)


def elf_machine(path: Path) -> str | None:
    try:
        header = path.read_bytes()[:20]
    except OSError:
        return None
    if len(header) < 20 or header[:4] != b"\x7fELF":
        return None
    endian = "<" if header[5] == 1 else ">"
    machine = struct.unpack(f"{endian}H", header[18:20])[0]
    return {62: "x86_64", 183: "aarch64"}.get(machine, f"elf-machine-{machine}")


def contains_calibrated_profile(path: Path, profile: str) -> bool:
    if not path.is_file():
        return False
    active = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        stripped = raw.strip()
        if raw.startswith("  ") and not raw.startswith("    ") and stripped.endswith(":"):
            active = stripped[:-1] == profile
            continue
        if active and stripped.startswith("calibrated:"):
            return stripped.split(":", 1)[1].strip().lower() == "true"
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--search", type=Path, default=Path("/dev/video0"))
    parser.add_argument("--tele", type=Path, default=Path("/dev/video1"))
    parser.add_argument(
        "--engine",
        type=Path,
        default=ROOT / "vision/models/engines/yolov8n_fp16.engine",
    )
    parser.add_argument("--allow-single-camera", action="store_true")
    parser.add_argument(
        "--runtime-only",
        action="store_true",
        help="skip compiler/build-tool checks when invoked by systemd",
    )
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    machine = platform.machine().lower()
    system = platform.system()
    binary = ROOT / "vision/build/dual_detect_node"
    binary_machine = elf_machine(binary)
    logger_binary = ROOT / "vision/build/detection_logger"
    logger_machine = elf_machine(logger_binary)
    calibration = ROOT / "vision/configs/calib_profiles.yaml"
    service = ROOT / "deploy/orin/systemd/actprove-passive-vision.service"
    logger_service = ROOT / "deploy/orin/systemd/actprove-passive-detection-logger.service"

    results = [
        check(system == "Linux", "Operating system", system, f"Linux required; found {system}"),
        check(
            machine in {"aarch64", "arm64"},
            "CPU architecture",
            machine,
            f"Jetson build requires aarch64; found {machine}",
        ),
        check(args.search.exists(), "SEARCH camera", str(args.search), f"missing {args.search}"),
        check(
            args.tele.exists(),
            "TELE camera",
            str(args.tele),
            f"missing {args.tele}",
            warning=args.allow_single_camera,
        ),
        check(args.engine.is_file(), "TensorRT engine", str(args.engine), f"missing {args.engine}"),
        check(binary.is_file(), "Observer binary", str(binary), f"missing {binary}"),
        check(
            binary_machine == "aarch64",
            "Observer binary architecture",
            "aarch64",
            f"expected aarch64, found {binary_machine or 'not an ELF binary'}",
        ),
        check(
            contains_calibrated_profile(calibration, "ELP_AR0234_SEARCH_100DEG"),
            "SEARCH calibration",
            "calibrated",
            "ELP SEARCH profile is not calibrated",
        ),
        check(service.is_file(), "Passive systemd unit", str(service), f"missing {service}"),
        check(
            logger_binary.is_file(),
            "Detection logger binary",
            str(logger_binary),
            f"missing {logger_binary}",
        ),
        check(
            logger_machine == "aarch64",
            "Detection logger architecture",
            "aarch64",
            f"expected aarch64, found {logger_machine or 'not an ELF binary'}",
        ),
        check(
            logger_service.is_file(),
            "Detection logger systemd unit",
            str(logger_service),
            f"missing {logger_service}",
        ),
    ]

    if not args.runtime_only:
        results.extend(
            [
                check(shutil.which("cmake") is not None, "CMake", "available", "cmake not found"),
                check(shutil.which("g++") is not None, "C++ compiler", "available", "g++ not found"),
            ]
        )

    tele_calibrated = contains_calibrated_profile(calibration, "ELP_AR0234_TELE_50MM")
    if args.allow_single_camera and not tele_calibrated:
        results.append(Check("TELE calibration", "WARN", "skipped for single-camera bench"))
    else:
        results.append(
            check(
                tele_calibrated,
                "TELE calibration",
                "calibrated",
                "ELP TELE profile is not calibrated",
            )
        )

    # A passive service must not gain access to serial/RC devices.
    service_text = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (service, logger_service)
        if path.is_file()
    )
    active_service_text = "\n".join(
        line.strip().lower()
        for line in service_text.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    )
    forbidden = ("ttyTHS", "mavlink", "sbus", "pwm", "gpio")
    exposed = [token for token in forbidden if token.lower() in active_service_text]
    results.append(
        check(
            not exposed,
            "No flight-control device access",
            "camera-only service",
            f"forbidden device/control token(s): {', '.join(exposed)}",
        )
    )

    failed = sum(result.status == "FAIL" for result in results)
    warned = sum(result.status == "WARN" for result in results)
    payload = {
        "scope": "passive_orin_vision",
        "ready": failed == 0,
        "flight_evidence": False,
        "root": str(ROOT),
        "checks": [asdict(result) for result in results],
        "summary": {"pass": len(results) - failed - warned, "warn": warned, "fail": failed},
    }
    if args.json:
        print(json.dumps(payload, indent=2))
    else:
        for result in results:
            print(f"{result.status:4}  {result.name}: {result.detail}")
        print(
            f"SUMMARY: {payload['summary']['pass']} PASS, {warned} WARN, {failed} FAIL"
        )
        print("PASS means passive observer prerequisites only; it is not flight evidence.")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
