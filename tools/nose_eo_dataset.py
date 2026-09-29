"""Local, operator-triggered dataset capture helpers for Nose EO demos.

This module deliberately contains no detector or flight-control integration.
It only stores explicitly submitted JPEG frames as hard-negative examples for
offline false-positive evaluation.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping

MAX_NEGATIVE_JPEG_BYTES = 8 * 1024 * 1024
_SAFE_TOKEN_RE = re.compile(r"[^a-z0-9_-]+")
_IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff"}


def safe_token(value: str, fallback: str = "other", max_len: int = 32) -> str:
    token = _SAFE_TOKEN_RE.sub("_", value.strip().lower()).strip("_")
    return (token or fallback)[:max_len]


def negative_sample_count(out_dir: Path) -> int:
    if not out_dir.is_dir():
        return 0
    return sum(
        1
        for path in out_dir.iterdir()
        if path.is_file() and path.suffix.lower() in _IMAGE_SUFFIXES and not path.name.startswith(".")
    )


def save_negative_sample(
    jpeg: bytes,
    out_dir: Path,
    *,
    reason: str = "false_cue",
    source: str = "nose_eo_browser",
    width: int | None = None,
    height: int | None = None,
    cue: Mapping[str, Any] | None = None,
    session_id: str = "",
    frame_index: int | None = None,
    now: datetime | None = None,
) -> dict[str, Any]:
    """Atomically store one explicit hard-negative JPEG and metadata row.

    Identical JPEG bytes are deduplicated by SHA-256. The returned path is
    relative to ``out_dir`` so the API does not expose host filesystem paths.
    """
    if not jpeg:
        raise ValueError("empty JPEG body")
    if len(jpeg) > MAX_NEGATIVE_JPEG_BYTES:
        raise ValueError(f"JPEG exceeds {MAX_NEGATIVE_JPEG_BYTES} byte limit")
    if not jpeg.startswith(b"\xff\xd8"):
        raise ValueError("body is not a JPEG")

    out_dir.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(jpeg).hexdigest()
    stamp_dt = now or datetime.now(timezone.utc)
    stamp = stamp_dt.astimezone(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    category = safe_token(reason)
    filename = f"negative_{category}_{stamp}_{digest[:12]}.jpg"
    destination = out_dir / filename

    duplicate = False
    existing = next(out_dir.glob(f"negative_*_{digest[:12]}.jpg"), None)
    if existing is not None:
        destination = existing
        duplicate = True
    else:
        fd, tmp_name = tempfile.mkstemp(prefix=".negative_", suffix=".jpg", dir=out_dir)
        try:
            with os.fdopen(fd, "wb") as handle:
                handle.write(jpeg)
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(tmp_name, destination)
        except Exception:
            try:
                os.unlink(tmp_name)
            except OSError:
                pass
            raise

    event = {
        "image": destination.name,
        "sha256": digest,
        "bytes": len(jpeg),
        "width": width,
        "height": height,
        "reason": category,
        "source": safe_token(source, fallback="nose_eo_browser", max_len=48),
        "session_id": safe_token(session_id, fallback="single", max_len=64),
        "frame_index": frame_index,
        "cue": dict(cue or {}),
        "captured_at": stamp_dt.astimezone(timezone.utc).isoformat(),
        "duplicate": duplicate,
    }
    if not duplicate:
        with (out_dir / "capture_meta.jsonl").open("a", encoding="utf-8") as metadata:
            metadata.write(json.dumps(event, ensure_ascii=True, sort_keys=True) + "\n")

    return {
        "ok": True,
        "saved": not duplicate,
        "duplicate": duplicate,
        "image": destination.name,
        "sha256": digest,
        "count": negative_sample_count(out_dir),
    }
