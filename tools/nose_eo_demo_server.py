#!/usr/bin/env python3
"""Nose EO demo detection sidecar for ACTPROVE GCS UI.

Grabs frames from a local webcam (prefer GoPro Webcam), runs YOLOv8n COCO
(demo detector), estimates geometric closing range, and serves JSON / optional
MJPEG for the Vite GCS at http://127.0.0.1:5173/.

DEMO ONLY — range is a pinhole estimate (L=3.0 m, HFOV≈120°), not radar/LiDAR.
Real class-gated Shahed/Geran/Gerbera models are not used here.

Mac quick start:
  cd /workspace/actprove-drone
  python3 -m venv .venv-nose-eo && source .venv-nose-eo/bin/activate
  pip install ultralytics opencv-python fastapi uvicorn
  # optional: reuse vision/.venv which already has ultralytics+opencv
  python tools/nose_eo_demo_server.py --port 8765

Endpoints:
  GET  /health
  GET  /detect          → latest detection JSON (incl. jpeg_hz / est_jump_ratio / range_status)
  POST /detect/frame    → raw image/jpeg body + X-Target-Size-M / X-HFOV-Deg → detect+EST
  POST /dataset/negative/frame → explicitly save one hard-negative JPEG
  GET  /mjpeg           → multipart JPEG stream (optional UI fallback)
  WS   /ws              → streaming detection JSON
  GET/POST /config      → calib profile + EST knobs (DEMO_GOPRO ≠ LOCK_CAM_A_40MM)
  POST /calib/hfov      → calibrate HFOV at known range (per active profile)
  POST /track/init      → start CSRT/KCF/MIL/TMPL track on ROI (+ optional jpeg)
  POST /track/update    → step tracker (+ preferred browser jpeg) → new box
  GET  /track           → current DEMO track snapshot
  POST /track/stop      → clear visual track
"""
from __future__ import annotations

import argparse
import asyncio
import math
import os
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# Module-level for FastAPI DI (future annotations + nested build_app imports
# would otherwise treat `request: Request` as a required query param).
try:
    from fastapi import Request as FastAPIRequest
except ImportError:  # sidecar deps optional until runtime
    FastAPIRequest = Any  # type: ignore[misc,assignment]

from nose_eo_est import (
    DEFAULT_PROFILE,
    LOST_HOLD_S,
    EstFilterState,
    EstKnobs,
    apply_est_filter,
    calibrate_hfov_deg,
    classify_box_status,
    estimate_range_m as est_range_from_width,
    load_calib_profiles,
    lost_fails_for_hz,
    profile_hfov_deg,
    profile_label,
    profile_lock_eligible,
)

from nose_eo_track_helpers import (
    AREA_JUMP_MAX,
    AREA_JUMP_MIN,
    ROI_EXPAND_BASE,
    ROI_EXPAND_GROWTH_TRIGGER,
    ROI_EXPAND_MAX,
    area_growth_ratio,
    area_jump_ok,
    iou_xywh,
    motion_vector_coherence,
    reacquire_roi_norm,
    roi_expand_for_closing,
    select_reacquire_candidate,
)
from nose_eo_dataset import MAX_NEGATIVE_JPEG_BYTES, negative_sample_count, save_negative_sample

# ---------------------------------------------------------------------------
# Demo geometry / EST knobs (profiles: vision/configs/calib_profiles.yaml)
# Hard rule: DEMO_GOPRO (~120°) ≠ LOCK_CAM_A_40MM (~10°) — never mix HFOV.
# ---------------------------------------------------------------------------
# Legacy aliases (detect path); EST track path uses KNOBS + horizontal known width.
TARGET_LENGTH_M = 3.0  # legacy detect-path length assumption (Shahed-class)
HFOV_DEG = 120.0  # legacy default = DEMO_GOPRO; prefer KNOBS.effective_hfov_deg()
CLOSING_EPS_M = 0.15  # range must drop by this much to flag closing
RANGE_NEAR_M = 50.0  # UI emphasize threshold (reported in payload)

PROFILES = load_calib_profiles()
KNOBS = EstKnobs(profile_id=DEFAULT_PROFILE)
EST_FILTER = EstFilterState()
_KNOBS_LOCK = threading.Lock()

# Conservative semantic candidates for automatic demo acquisition. People,
# birds, kites and sports balls remain visible in detector output but must not
# seed an automatic visual track.
CANDIDATE_CLASSES = {
    "car",
    "truck",
    "bus",
    "motorcycle",
    "bicycle",
    "airplane",
    "motion",
}

ROAD_VEHICLE_CLASSES = {"car", "truck", "bus", "motorcycle", "bicycle"}
MIN_ROAD_AUTO_CONF = 0.35
MIN_OTHER_AUTO_CONF = 0.40
VEHICLE_WIDTH_M = {
    "car": 1.8,
    "truck": 2.5,
    "bus": 2.5,
    "motorcycle": 0.8,
    "bicycle": 0.6,
}

PREFERRED_CAM_RE = re.compile(r"GoPro|Webcam|USB", re.I)

ROOT = Path(__file__).resolve().parents[1]
NEGATIVE_INBOX = ROOT / "vision" / "data" / "inbox" / "negative"
DEFAULT_WEIGHTS = [
    ROOT / "vision" / "yolov8n.pt",
    Path("yolov8n.pt"),
]


@dataclass
class Box:
    x: float
    y: float
    w: float
    h: float
    cls: str
    conf: float

    def as_dict(self) -> dict[str, Any]:
        return {
            "x": round(self.x, 5),
            "y": round(self.y, 5),
            "w": round(self.w, 5),
            "h": round(self.h, 5),
            "cls": self.cls,
            "conf": round(self.conf, 4),
        }


@dataclass
class DetectState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    ok: bool = False
    boxes: list[Box] = field(default_factory=list)
    range_m: float | None = None
    closing: bool = False
    fps: float = 0.0
    frame_w: int = 0
    frame_h: int = 0
    camera_label: str = ""
    camera_index: int = -1
    detector: str = "none"
    error: str | None = None
    ts: float = 0.0
    jpeg: bytes | None = None
    prev_range_m: float | None = None
    # Lab-card / Mac GCS POST /detect/frame evidence
    jpeg_hz: float | None = None
    est_jump_ratio: float | None = None
    range_status: str = "none"  # ok | clipped | too_small | jump_rejected | none
    profile_id: str = DEFAULT_PROFILE
    known_width_m: float = 0.08
    hfov_used_deg: float | None = None

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            with _KNOBS_LOCK:
                kn = KNOBS.as_public_dict(PROFILES)
            hfov = self.hfov_used_deg if self.hfov_used_deg is not None else kn.get("hfov_deg", HFOV_DEG)
            return {
                "ok": self.ok,
                "boxes": [b.as_dict() for b in self.boxes],
                "range_m": None if self.range_m is None else round(self.range_m, 2),
                "closing": self.closing,
                "fps": round(self.fps, 1),
                "frame_w": self.frame_w,
                "frame_h": self.frame_h,
                "camera_label": self.camera_label,
                "camera_index": self.camera_index,
                "detector": self.detector,
                "label": "TARGET (demo)",
                "demo": True,
                "lock_eligible": False,
                "hfov_deg": hfov,
                "target_length_m": TARGET_LENGTH_M,
                "known_width_m": round(float(self.known_width_m), 4),
                "near_m": RANGE_NEAR_M,
                "range_status": self.range_status,
                "profile_id": self.profile_id or kn.get("profile_id", DEFAULT_PROFILE),
                "profile_label": kn.get("profile_label", "DEMO"),
                "jpeg_hz": None if self.jpeg_hz is None else round(self.jpeg_hz, 2),
                "est_jump_ratio": None
                if self.est_jump_ratio is None
                else round(self.est_jump_ratio, 3),
                "error": self.error,
                "ts": self.ts,
                "note": "DEMO detect EST — SEARCH bias only; not class Lock / not radar",
            }


STATE = DetectState()

# Runtime shared with DetectorWorker (YOLO) + POST /detect/frame
_YOLO_RUNTIME: dict[str, Any] = {"model": None, "names": {}, "conf": 0.20, "detector": "none"}
_DETECT_PREV_GRAY: Any | None = None
_DETECT_FRAME_TS: float | None = None
DETECT_EST_FILTER = EstFilterState()
_MOTION_BG: dict[str, tuple[tuple[int, ...], Any]] = {}
_MOTION_LOCK = threading.Lock()
_TRACK_MOTION_PREV_GRAY: Any | None = None


def resolve_frame_hfov(header_hfov: float | None) -> tuple[float, str]:
    """Resolve HFOV for POST /detect/frame.

    Hard rule: a narrow TELE profile never uses a wide-camera HFOV.
    Prefer active profile hfov (or calib knobs override). Header X-HFOV-Deg may
    override only when:
      - profile is DEMO_GOPRO (lab), OR
      - profile is LOCK_CAM_A_40MM and header is narrow (<60°) — GoPro-wide headers
        are ignored so Cam A EST cannot inherit ~120°.
    """
    with _KNOBS_LOCK:
        pid = str(KNOBS.profile_id)
        profile_hfov = float(profile_hfov_deg(pid, PROFILES))
        effective = float(KNOBS.effective_hfov_deg(PROFILES))

    if profile_hfov < 60.0:
        if header_hfov is not None and header_hfov > 0:
            if float(header_hfov) >= 60.0:
                # Refuse GoPro-class wide FoV on Cam A profile — keep profile/calib
                return effective if effective < 60.0 else profile_hfov, "profile_ignored_gopro_header"
            return float(header_hfov), "header_narrow_ok"
        # Prefer profile ~10° (or calib override); never legacy HFOV_DEG=120
        return (effective if effective < 60.0 else profile_hfov), "profile_or_calib"

    # DEMO_GOPRO (and any other non-Cam-A profile)
    if header_hfov is not None and header_hfov > 0:
        return float(header_hfov), "header"
    return effective, "profile_or_calib"


def resolve_target_size_m(header_size: float | None) -> float:
    if header_size is not None and header_size > 0:
        return float(header_size)
    with _KNOBS_LOCK:
        return float(KNOBS.known_width_m)


def _yolo_boxes_on_frame(frame: Any, w: int, h: int) -> list[Box]:
    model = _YOLO_RUNTIME.get("model")
    names = _YOLO_RUNTIME.get("names") or {}
    conf = float(_YOLO_RUNTIME.get("conf") or 0.35)
    boxes: list[Box] = []
    if model is None:
        return boxes
    try:
        results = model.predict(frame, conf=conf, verbose=False)
        if not results:
            return boxes
        r0 = results[0]
        if r0.boxes is None or not len(r0.boxes):
            return boxes
        xyxy = r0.boxes.xyxy.cpu().numpy()
        confs = r0.boxes.conf.cpu().numpy()
        clss = r0.boxes.cls.cpu().numpy().astype(int)
        for (x1, y1, x2, y2), cf, ci in zip(xyxy, confs, clss):
            cls_name = str(names.get(int(ci), str(ci)))
            bw = max(0.0, float(x2 - x1))
            bh = max(0.0, float(y2 - y1))
            boxes.append(
                Box(
                    x=float(x1) / w,
                    y=float(y1) / h,
                    w=bw / w,
                    h=bh / h,
                    cls=cls_name,
                    conf=float(cf),
                )
            )
    except Exception:  # noqa: BLE001
        return []
    return boxes


def process_posted_detect_frame(
    cv2: Any,
    frame: Any,
    *,
    header_hfov: float | None = None,
    header_size: float | None = None,
    stream_id: str = "posted-detect",
) -> dict[str, Any]:
    """Decode path already done — YOLO/motion → EST → update STATE → snapshot.

    Uses profile HFOV unless header override is allowed (see resolve_frame_hfov).
    """
    global _DETECT_PREV_GRAY, _DETECT_FRAME_TS, DETECT_EST_FILTER

    h, w = frame.shape[:2]
    boxes = _yolo_boxes_on_frame(frame, w, h)
    detector = str(_YOLO_RUNTIME.get("detector") or "none")
    # A detector result for an unrelated static class must not suppress the
    # motion fallback. This matters for distant cars that YOLO misses while it
    # still sees a traffic light, bench, etc.
    motion_boxes, _DETECT_PREV_GRAY = motion_fallback(
        cv2,
        frame,
        _DETECT_PREV_GRAY,
        stream_id=stream_id,
    )
    for motion_box in motion_boxes:
        if not any(iou_xywh(motion_box, detected) >= 0.35 for detected in boxes):
            boxes.append(motion_box)
    if motion_boxes:
        detector = "motion" if detector == "none" else f"{detector}+motion"

    target = pick_target_box(boxes)
    hfov, hfov_src = resolve_frame_hfov(header_hfov)
    known_w = resolve_target_size_m(header_size)
    if header_size is None and target is not None:
        known_w = VEHICLE_WIDTH_M.get(target.cls, known_w)

    with _KNOBS_LOCK:
        alpha = float(KNOBS.ema_alpha)
        alpha_fast = float(KNOBS.ema_alpha_fast)
        max_jump = float(KNOBS.max_jump_ratio)
        min_frac = float(KNOBS.min_box_frac)
        closing_eps = float(KNOBS.closing_eps_m)
        profile_id = str(KNOBS.profile_id)

    range_m: float | None = None
    closing = False
    range_status = "none"
    jump_ratio: float | None = None

    if target is not None and not (target.cls == "motion" and header_size is None):
        status = classify_box_status(target.w, target.h, target.x, target.y, min_frac)
        raw: float | None = None
        if status == "ok":
            raw = est_range_from_width(target.w * float(w), w, known_w, hfov)
        filt, status2, DETECT_EST_FILTER = apply_est_filter(
            raw,
            DETECT_EST_FILTER,
            range_status=status,
            alpha=alpha,
            alpha_fast=alpha_fast,
            max_jump_ratio=max_jump,
            closing_eps_m=closing_eps,
        )
        range_m = filt
        range_status = status2
        closing = bool(getattr(DETECT_EST_FILTER, "closing", False))
        with STATE.lock:
            prev_r = STATE.prev_range_m
        if filt is not None and prev_r is not None and prev_r > 1e-6:
            jump_ratio = float(filt) / float(prev_r)
    else:
        DETECT_EST_FILTER = EstFilterState()
        if target is not None and target.cls == "motion":
            range_status = "unknown_size"

    now = time.time()
    jpeg_hz: float | None = None
    if _DETECT_FRAME_TS is not None:
        dt = now - _DETECT_FRAME_TS
        if dt > 1e-4:
            jpeg_hz = 1.0 / dt
    _DETECT_FRAME_TS = now

    with STATE.lock:
        STATE.ok = True
        STATE.boxes = boxes
        STATE.range_m = range_m
        STATE.closing = closing
        STATE.frame_w = w
        STATE.frame_h = h
        STATE.ts = now
        STATE.detector = detector or STATE.detector or "motion"
        STATE.camera_label = STATE.camera_label or "browser-jpeg"
        STATE.error = None
        STATE.jpeg_hz = jpeg_hz
        STATE.est_jump_ratio = jump_ratio
        STATE.range_status = range_status
        STATE.profile_id = profile_id
        STATE.known_width_m = known_w
        STATE.hfov_used_deg = hfov
        if range_m is not None:
            STATE.prev_range_m = float(range_m)
        # keep fps as posted-frame rate estimate when available
        if jpeg_hz is not None:
            STATE.fps = float(jpeg_hz)

    snap = STATE.snapshot()
    snap["hfov_source"] = hfov_src
    snap["label"] = "TARGET (demo)"
    snap["demo"] = True
    return snap


# ---------------------------------------------------------------------------
# Visual accompany tracker (DEMO) — prefer browser JPEG frames (Mac TCC safe)
# ---------------------------------------------------------------------------
TRACK_LOST_FAILS = KNOBS.effective_lost_fails()  # ~2.5 s at the configured browser JPEG rate
TRACK_REDETECT_EVERY = 4
ROI_EXPAND = ROI_EXPAND_BASE  # adaptive via roi_expand_for_closing


@dataclass
class TrackState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    active: bool = False
    lost: bool = False
    ok: bool = False
    algo: str = "none"
    box: Box | None = None
    frame_w: int = 0
    frame_h: int = 0
    fail_count: int = 0
    update_n: int = 0
    ts: float = 0.0
    error: str | None = None
    # Lab-card fields (test scoring)
    jpeg_hz: float | None = None
    est_jump_ratio: float | None = None
    area_growth: float | None = None
    roi_expand: float | None = None
    prev_range_m: float | None = None
    # Monocular EST (DEMO — SEARCH bias only; never Lock)
    est_range_m: float | None = None
    range_status: str = "none"  # ok | clipped | too_small | jump_rejected | none
    closing: bool = False
    profile_id: str = DEFAULT_PROFILE
    target_cls: str | None = None
    owner_id: str | None = None
    semantic_miss_count: int = 0
    # Internal OpenCV objects (not serialized)
    _tracker: Any = None
    _template: Any = None  # gray crop for TMPL fallback
    _tmpl_size: tuple[int, int] = (0, 0)  # w, h of template
    _yolo_model: Any = None
    _yolo_names: dict[int, str] = field(default_factory=dict)
    _semantic_box: Box | None = None

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            with _KNOBS_LOCK:
                kn = KNOBS.as_public_dict(PROFILES)
            return {
                "ok": self.ok and self.active and not self.lost,
                "active": self.active,
                "lost": self.lost,
                "algo": self.algo,
                "box": None if self.box is None else self.box.as_dict(),
                "frame_w": self.frame_w,
                "frame_h": self.frame_h,
                "fail_count": self.fail_count,
                "update_n": self.update_n,
                "label": "DEMO track",
                "demo": True,
                "lock_eligible": False,  # DEMO accompany never raises Lock
                "est_range_m": None if self.est_range_m is None else round(self.est_range_m, 2),
                "range_status": self.range_status,
                "closing": self.closing,
                "profile_id": self.profile_id,
                "target_cls": self.target_cls,
                "owner_active": bool(self.owner_id),
                "semantic_miss_count": self.semantic_miss_count,
                "profile_label": kn.get("profile_label", "DEMO"),
                "hfov_deg": kn.get("hfov_deg"),
                "known_width_m": VEHICLE_WIDTH_M.get(
                    self.target_cls, kn.get("known_width_m")
                ),
                "error": self.error,
                "ts": self.ts,
                "jpeg_hz": None if self.jpeg_hz is None else round(self.jpeg_hz, 2),
                "est_jump_ratio": None
                if self.est_jump_ratio is None
                else round(self.est_jump_ratio, 3),
                "area_growth": None if self.area_growth is None else round(self.area_growth, 3),
                "roi_expand": None if self.roi_expand is None else round(self.roi_expand, 3),
                "note": "DEMO accompany + EST — SEARCH bias only; not class Lock",
            }


TRACK = TrackState()


def update_track_est(box: "Box | None", frame_w: int, frame_h: int) -> None:
    """Refresh TRACK monocular EST from box (horizontal known width).

    DEMO only — SEARCH bias. Status: ok | clipped | too_small | jump_rejected | none.
    Jump reject: |R/Rprev| > max_jump_ratio (default 2).
    """
    global EST_FILTER
    with _KNOBS_LOCK:
        kn = KNOBS
        known_w = float(kn.known_width_m)
        hfov = float(kn.effective_hfov_deg(PROFILES))
        alpha = float(kn.ema_alpha)
        alpha_fast = float(kn.ema_alpha_fast)
        max_jump = float(kn.max_jump_ratio)
        min_frac = float(kn.min_box_frac)
        closing_eps = float(kn.closing_eps_m)
        profile_id = str(kn.profile_id)

    with TRACK.lock:
        target_cls = TRACK.target_cls
    if target_cls == "motion":
        # A class-agnostic contour has no known physical width, so monocular
        # distance would be fabricated. Keep center/box tracking but mark EST.
        with TRACK.lock:
            TRACK.est_range_m = None
            TRACK.range_status = "unknown_size"
            TRACK.closing = False
            TRACK.profile_id = profile_id
        return
    if target_cls in VEHICLE_WIDTH_M:
        known_w = VEHICLE_WIDTH_M[target_cls]

    if box is None or frame_w <= 0:
        with TRACK.lock:
            TRACK.range_status = "none"
            TRACK.closing = False
            TRACK.profile_id = profile_id
        return

    status = classify_box_status(box.w, box.h, box.x, box.y, min_frac)
    raw: float | None = None
    if status == "ok":
        raw = est_range_from_width(box.w * float(frame_w), frame_w, known_w, hfov)

    filt, status2, EST_FILTER = apply_est_filter(
        raw,
        EST_FILTER,
        range_status=status,
        alpha=alpha,
        alpha_fast=alpha_fast,
        max_jump_ratio=max_jump,
        closing_eps_m=closing_eps,
    )
    with TRACK.lock:
        TRACK.est_range_m = filt
        TRACK.range_status = status2
        TRACK.closing = bool(getattr(EST_FILTER, "closing", False))
        TRACK.profile_id = profile_id
        if filt is not None and TRACK.prev_range_m is not None and TRACK.prev_range_m > 1e-6:
            TRACK.est_jump_ratio = float(filt) / float(TRACK.prev_range_m)
        else:
            TRACK.est_jump_ratio = None
        if filt is not None:
            TRACK.prev_range_m = float(filt)


def apply_knobs_patch(body: dict[str, Any]) -> dict[str, Any]:
    """Merge POST /config body into KNOBS. Profile switch clears hfov override."""
    global EST_FILTER
    allowed = {
        "profile_id", "known_width_m", "hfov_deg", "ema_alpha", "ema_alpha_fast",
        "max_jump_ratio", "min_box_frac", "roi_expand", "jpeg_max_w", "jpeg_quality",
        "jpeg_hz", "track_lost_fails", "track_redetect_every", "csrt_padding", "closing_eps_m",
    }
    with _KNOBS_LOCK:
        if "profile_id" in body and body["profile_id"] is not None:
            pid = str(body["profile_id"])
            if pid not in PROFILES:
                raise ValueError(f"unknown profile_id {pid}; known={list(PROFILES)}")
            if pid != KNOBS.profile_id:
                KNOBS.profile_id = pid
                KNOBS.hfov_deg = None  # hard rule: never carry GoPro HFOV onto Cam A
                EST_FILTER = EstFilterState()
        for key in allowed:
            if key == "profile_id" or key not in body or body[key] is None:
                continue
            if not hasattr(KNOBS, key):
                continue
            val = body[key]
            if key in ("jpeg_max_w", "track_lost_fails", "track_redetect_every"):
                setattr(KNOBS, key, int(val))
            elif key == "hfov_deg":
                if val is None:
                    KNOBS.hfov_deg = None
                else:
                    hv = float(val)
                    profile_hfov = float(profile_hfov_deg(KNOBS.profile_id, PROFILES))
                    # Hard rule: never park a wide FoV on any narrow TELE profile.
                    if profile_hfov < 60.0 and hv >= 60.0:
                        raise ValueError(
                            f"{KNOBS.profile_id} refuses wide-camera HFOV (≥60°); "
                            "use its narrow profile value or a measured calibration"
                        )
                    KNOBS.hfov_deg = hv
            else:
                setattr(KNOBS, key, float(val) if not isinstance(getattr(KNOBS, key), str) else val)
        # Keep module-level lost-fails alias fresh
        global TRACK_LOST_FAILS, TRACK_REDETECT_EVERY
        TRACK_LOST_FAILS = KNOBS.effective_lost_fails()
        TRACK_REDETECT_EVERY = max(1, int(KNOBS.track_redetect_every))
        return KNOBS.as_public_dict(PROFILES)




def _import_cv2() -> Any:
    import cv2  # type: ignore

    return cv2


def decode_jpeg_b64(cv2: Any, b64: str) -> Any:
    import base64

    import numpy as np

    raw = base64.b64decode(b64)
    arr = np.frombuffer(raw, dtype=np.uint8)
    frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if frame is None:
        raise ValueError("Failed to decode jpeg_b64")
    return frame


def decode_jpeg_bytes(cv2: Any, raw: bytes) -> Any:
    import numpy as np

    if not raw:
        raise ValueError("Empty image/jpeg body")
    arr = np.frombuffer(raw, dtype=np.uint8)
    frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if frame is None:
        raise ValueError("Failed to decode image/jpeg body")
    return frame


def norm_to_xywh_px(
    x: float, y: float, w: float, h: float, fw: int, fh: int, normalized: bool
) -> tuple[int, int, int, int]:
    if normalized:
        px = int(round(x * fw))
        py = int(round(y * fh))
        pw = int(round(w * fw))
        ph = int(round(h * fh))
    else:
        px, py, pw, ph = int(x), int(y), int(w), int(h)
    pw = max(8, min(pw, fw - 1))
    ph = max(8, min(ph, fh - 1))
    px = max(0, min(px, fw - pw))
    py = max(0, min(py, fh - ph))
    return px, py, pw, ph


def xywh_px_to_norm(px: int, py: int, pw: int, ph: int, fw: int, fh: int) -> Box:
    return Box(
        x=px / fw,
        y=py / fh,
        w=pw / fw,
        h=ph / fh,
        cls="track",
        conf=0.9,
    )


def create_cv_tracker(cv2: Any, prefer: str | None = None) -> tuple[Any, str]:
    """Prefer CSRT → KCF → MIL (opencv-contrib for CSRT/KCF)."""
    order = ["CSRT", "KCF", "MIL"]
    if prefer:
        pref = prefer.upper()
        order = [pref] + [a for a in order if a != pref]

    modules: list[Any] = [cv2]
    legacy = getattr(cv2, "legacy", None)
    if legacy is not None:
        modules.append(legacy)

    for name in order:
        for mod in modules:
            factory = getattr(mod, f"Tracker{name}_create", None)
            if factory is None:
                continue
            try:
                if name == "CSRT":
                    # Prefer params with larger padding for fast translate / closing.
                    params = None
                    for pmod in modules:
                        P = getattr(pmod, "TrackerCSRT_Params", None)
                        if P is None:
                            continue
                        try:
                            params = P()
                            with _KNOBS_LOCK:
                                pad = float(KNOBS.csrt_padding)
                            if hasattr(params, "padding"):
                                params.padding = pad
                            if hasattr(params, "template_size"):
                                # slightly larger template helps under scale change
                                params.template_size = max(int(getattr(params, "template_size", 200) or 200), 200)
                            break
                        except Exception:  # noqa: BLE001
                            params = None
                    if params is not None:
                        try:
                            return factory(params), name
                        except TypeError:
                            return factory(), name
                return factory(), name
            except Exception:  # noqa: BLE001
                continue
    return None, "TMPL"


def init_template_tracker(cv2: Any, frame: Any, xywh: tuple[int, int, int, int]) -> tuple[Any, tuple[int, int]]:
    x, y, w, h = xywh
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    tmpl = gray[y : y + h, x : x + w].copy()
    return tmpl, (w, h)


def update_template_tracker(
    cv2: Any,
    frame: Any,
    tmpl: Any,
    tmpl_wh: tuple[int, int],
    last_xywh: tuple[int, int, int, int],
) -> tuple[bool, tuple[int, int, int, int], float]:
    """NCC match in expanded ROI around last box — always available w/ base OpenCV."""
    import numpy as np

    fh, fw = frame.shape[:2]
    tw, th = tmpl_wh
    lx, ly, lw, lh = last_xywh
    cx = lx + lw / 2.0
    cy = ly + lh / 2.0
    with _KNOBS_LOCK:
        _exp = float(KNOBS.roi_expand)
    search_w = int(max(lw, tw) * _exp)
    search_h = int(max(lh, th) * _exp)
    x0 = max(0, int(cx - search_w / 2))
    y0 = max(0, int(cy - search_h / 2))
    x1 = min(fw, x0 + search_w)
    y1 = min(fh, y0 + search_h)
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    roi = gray[y0:y1, x0:x1]
    if roi.shape[0] < th + 2 or roi.shape[1] < tw + 2:
        return False, last_xywh, 0.0
    if tmpl.shape[0] > roi.shape[0] or tmpl.shape[1] > roi.shape[1]:
        return False, last_xywh, 0.0
    res = cv2.matchTemplate(roi, tmpl, cv2.TM_CCOEFF_NORMED)
    _, max_val, _, max_loc = cv2.minMaxLoc(res)
    conf = float(max_val)
    if conf < 0.28:
        return False, last_xywh, conf
    nx = x0 + int(max_loc[0])
    ny = y0 + int(max_loc[1])
    return True, (nx, ny, tw, th), conf


def try_yolo_reacquire(
    cv2: Any,
    frame: Any,
    last_box: Box,
    model: Any,
    names: dict[int, str],
    conf_thr: float = MIN_ROAD_AUTO_CONF,
    roi_expand: float | None = None,
    prev_box: Box | None = None,
    target_cls: str | None = None,
) -> Box | None:
    """YOLO re-acquire in expanded ROI around last track box (DEMO).

    Optional roi_expand overrides ROI_EXPAND (adaptive pad for fast closing).
    prev_box is used only for area-growth context by callers; jump reject uses
    last_box vs candidate (spike/collapse).
    """
    if model is None:
        return None
    expand = float(ROI_EXPAND if roi_expand is None else roi_expand)
    # prev_box reserved for callers / future growth diagnostics
    _ = prev_box
    fh, fw = frame.shape[:2]
    x0, y0, x1, y1 = reacquire_roi_norm(last_box, expand)
    px0, py0 = int(x0 * fw), int(y0 * fh)
    px1, py1 = int(x1 * fw), int(y1 * fh)
    crop = frame[py0:py1, px0:px1]
    if crop.size == 0:
        return None
    try:
        results = model.predict(crop, conf=conf_thr, verbose=False)
    except Exception:  # noqa: BLE001
        return None
    if not results:
        return None
    r0 = results[0]
    if r0.boxes is None or len(r0.boxes) == 0:
        return None
    xyxy = r0.boxes.xyxy.cpu().numpy()
    confs = r0.boxes.conf.cpu().numpy()
    clss = r0.boxes.cls.cpu().numpy().astype(int)
    cands: list[Box] = []
    for (x1a, y1a, x2a, y2a), cf, ci in zip(xyxy, confs, clss):
        cls_name = str(names.get(int(ci), str(ci)))
        if cls_name not in CANDIDATE_CLASSES:
            continue
        if target_cls and target_cls != "track" and cls_name != target_cls:
            continue
        bx = (px0 + float(x1a)) / fw
        by = (py0 + float(y1a)) / fh
        bw = max(0.0, float(x2a - x1a)) / fw
        bh = max(0.0, float(y2a - y1a)) / fh
        cands.append(
            Box(
                x=bx,
                y=by,
                w=bw,
                h=bh,
                cls=cls_name,
                conf=float(cf),
            )
        )
    best = select_reacquire_candidate(
        cands,
        last_box,
        iou_min=0.03 if target_cls else 0.12,
        max_area_ratio=AREA_JUMP_MAX,
        min_area_ratio=AREA_JUMP_MIN,
    )
    if best is None:
        return None
    # The semantic label is retained for reacquisition/range only; this remains
    # a DEMO visual track and never raises a flight Lock by itself.
    return best



def resolve_frame(cv2: Any, body: dict[str, Any]) -> Any:
    """Prefer posted browser jpeg; else last detect JPEG from camera worker."""
    b64 = body.get("jpeg_b64") or body.get("frame_jpeg_b64") or body.get("jpeg_base64")
    if isinstance(b64, str) and b64.strip():
        # Allow data-URL prefix
        if "," in b64 and b64.strip().startswith("data:"):
            b64 = b64.split(",", 1)[1]
        return decode_jpeg_b64(cv2, b64)
    with STATE.lock:
        jpeg = STATE.jpeg
    if jpeg:
        import numpy as np

        arr = np.frombuffer(jpeg, dtype=np.uint8)
        frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        if frame is not None:
            return frame
    raise ValueError(
        "No frame available. POST jpeg_b64 from the browser video "
        "(preferred on Mac when OpenCV cannot open the camera)."
    )


def track_init(body: dict[str, Any], frame_override: Any | None = None) -> dict[str, Any]:
    global _TRACK_MOTION_PREV_GRAY
    try:
        cv2 = _import_cv2()
    except ImportError:
        return {
            "ok": False,
            "active": False,
            "lost": True,
            "error": "OpenCV (cv2) missing. pip install opencv-python",
            "label": "DEMO track",
        }

    try:
        frame = frame_override if frame_override is not None else resolve_frame(cv2, body)
    except Exception as exc:  # noqa: BLE001
        return {
            "ok": False,
            "active": False,
            "lost": True,
            "error": str(exc),
            "label": "DEMO track",
        }

    fh, fw = frame.shape[:2]
    _, _TRACK_MOTION_PREV_GRAY = motion_fallback(
        cv2,
        frame,
        None,
        stream_id="track",
    )
    normalized = bool(body.get("normalized", True))
    try:
        x = float(body["x"])
        y = float(body["y"])
        w = float(body["w"])
        h = float(body["h"])
    except (KeyError, TypeError, ValueError):
        return {
            "ok": False,
            "active": False,
            "lost": True,
            "error": "Need x,y,w,h in body",
            "label": "DEMO track",
        }

    xywh = norm_to_xywh_px(x, y, w, h, fw, fh, normalized)
    prefer = body.get("algo")
    tracker, algo = create_cv_tracker(cv2, prefer if isinstance(prefer, str) else None)

    tmpl = None
    tmpl_wh = (xywh[2], xywh[3])
    if tracker is not None:
        try:
            # OpenCV trackers want (x,y,w,h). init() often returns None (success).
            ok_init = tracker.init(frame, xywh)
            if ok_init is False:
                tracker, algo = None, "TMPL"
        except Exception:  # noqa: BLE001
            tracker, algo = None, "TMPL"

    if tracker is None or algo == "TMPL":
        algo = "TMPL"
        tmpl, tmpl_wh = init_template_tracker(cv2, frame, xywh)
        tracker = None
    else:
        # Seed template for soft fallback if CV tracker drifts
        tmpl, tmpl_wh = init_template_tracker(cv2, frame, xywh)

    box = xywh_px_to_norm(*xywh, fw, fh)
    box.conf = 1.0

    raw_target_cls = body.get("target_cls") or body.get("cls")
    raw_owner = body.get("client_id")
    owner_id = str(raw_owner).strip()[:128] if raw_owner else None
    target_cls = str(raw_target_cls) if raw_target_cls in CANDIDATE_CLASSES else None
    if not target_cls:
        # Auto/click init normally follows a recent detector box. Recover its
        # semantic class so a car can be reacquired after a short occlusion.
        with STATE.lock:
            recent_boxes = list(STATE.boxes)
        if recent_boxes:
            nearest = max(recent_boxes, key=lambda b: iou_xywh(box, b))
            semantic_conf_ok = (
                nearest.cls == "motion"
                or (
                    nearest.cls in ROAD_VEHICLE_CLASSES
                    and nearest.conf >= MIN_ROAD_AUTO_CONF
                )
                or nearest.conf >= MIN_OTHER_AUTO_CONF
            )
            if iou_xywh(box, nearest) >= 0.15 and semantic_conf_ok:
                target_cls = nearest.cls

    # Reuse the detector's already-loaded model. Constructing a second YOLO
    # instance on every Capture caused multi-second stalls and extra memory.
    yolo_model = _YOLO_RUNTIME.get("model")
    yolo_names = _YOLO_RUNTIME.get("names") or {}

    with TRACK.lock:
        TRACK.active = True
        TRACK.lost = False
        TRACK.ok = True
        TRACK.algo = algo
        TRACK.box = box
        TRACK.frame_w = fw
        TRACK.frame_h = fh
        TRACK.fail_count = 0
        TRACK.update_n = 0
        TRACK.ts = time.time()
        TRACK.error = None
        TRACK.target_cls = target_cls
        TRACK.owner_id = owner_id
        TRACK.semantic_miss_count = 0
        TRACK._semantic_box = box if target_cls in ROAD_VEHICLE_CLASSES else None
        TRACK._tracker = tracker
        TRACK._template = tmpl
        TRACK._tmpl_size = tmpl_wh
        TRACK._yolo_model = yolo_model
        TRACK._yolo_names = yolo_names

    update_track_est(box, fw, fh)
    snap = TRACK.snapshot()
    snap["note"] = "DEMO visual accompany — SEARCH bias only; not class-gated Lock"
    return snap


def track_update(
    body: dict[str, Any] | None = None,
    frame_override: Any | None = None,
) -> dict[str, Any]:
    global _TRACK_MOTION_PREV_GRAY
    body = body or {}
    raw_owner = body.get("client_id")
    owner_id = str(raw_owner).strip()[:128] if raw_owner else None
    with TRACK.lock:
        if TRACK.owner_id and owner_id and owner_id != TRACK.owner_id:
            return {
                "ok": False,
                "active": False,
                "lost": False,
                "box": None,
                "label": "DEMO track",
                "error": "track belongs to another GCS tab",
                "owner_conflict": True,
            }
        if not TRACK.active:
            return {
                "ok": False,
                "active": False,
                "lost": True,
                "box": None,
                "label": "DEMO track",
                "error": "No active track — POST /track/init first",
            }

    try:
        cv2 = _import_cv2()
    except ImportError:
        return {
            "ok": False,
            "active": False,
            "lost": True,
            "error": "OpenCV missing",
            "label": "DEMO track",
        }

    try:
        frame = frame_override if frame_override is not None else resolve_frame(cv2, body)
    except Exception as exc:  # noqa: BLE001
        with TRACK.lock:
            TRACK.error = str(exc)
            TRACK.fail_count += 1
            if TRACK.fail_count >= TRACK_LOST_FAILS:
                TRACK.lost = True
                TRACK.ok = False
        # snapshot() acquires TRACK.lock itself. Calling it from inside the
        # critical section deadlocks the /track/update request after a frame
        # capture failure and permanently stops the browser polling loop.
        return TRACK.snapshot()

    fh, fw = frame.shape[:2]
    motion_candidates, _TRACK_MOTION_PREV_GRAY = motion_fallback(
        cv2,
        frame,
        _TRACK_MOTION_PREV_GRAY,
        stream_id="track",
    )
    with TRACK.lock:
        tracker = TRACK._tracker
        algo = TRACK.algo
        tmpl = TRACK._template
        tmpl_wh = TRACK._tmpl_size
        last = TRACK.box
        yolo_model = TRACK._yolo_model
        yolo_names = TRACK._yolo_names
        update_n = TRACK.update_n
        prev_ts = TRACK.ts
        target_cls = TRACK.target_cls
        semantic_miss_count = TRACK.semantic_miss_count
        semantic_box = TRACK._semantic_box

    if last is None:
        return {
            "ok": False,
            "active": False,
            "lost": True,
            "error": "Track has no box",
            "label": "DEMO track",
        }

    last_xywh = norm_to_xywh_px(last.x, last.y, last.w, last.h, fw, fh, True)
    ok = False
    new_xywh = last_xywh
    conf = 0.5

    if tracker is not None and algo != "TMPL":
        try:
            ok, bbox = tracker.update(frame)
            if ok:
                x, y, w, h = [int(v) for v in bbox]
                new_xywh = (
                    max(0, x),
                    max(0, y),
                    max(8, w),
                    max(8, h),
                )
                conf = 0.85
        except Exception:  # noqa: BLE001
            ok = False

    if not ok:
        # Template fallback (also primary when algo=TMPL)
        if tmpl is None:
            tmpl, tmpl_wh = init_template_tracker(cv2, frame, last_xywh)
        ok, new_xywh, conf = update_template_tracker(cv2, frame, tmpl, tmpl_wh, last_xywh)
        if ok and algo != "TMPL":
            # Soft degrade note — keep original algo label with tmpl assist
            pass

    box = xywh_px_to_norm(*new_xywh, fw, fh)
    box.conf = max(0.05, min(1.0, conf if ok else 0.1))

    # Area growth vs previous box → adaptive ROI pad for fast closing
    growth = area_growth_ratio(box if ok else last, last)
    closing_flag = False
    with STATE.lock:
        closing_flag = bool(STATE.closing)
    # Also treat strong area growth as closing for pad purposes
    if growth > ROI_EXPAND_GROWTH_TRIGGER:
        closing_flag = True
    adapt_expand = roi_expand_for_closing(ROI_EXPAND, growth, closing_flag, max_expand=ROI_EXPAND_MAX)

    # Periodic / on-fail YOLO re-acquire in expanded ROI
    # Periodically correct tracker drift even when OpenCV still reports ok.
    # CSRT/KCF do not expose a useful confidence here, so waiting for ok=false
    # lets a box remain attached to a tree after the vehicle reappears.
    need_redetect = (not ok) or (
        update_n > 0
        and (target_cls == "motion" or update_n % TRACK_REDETECT_EVERY == 0)
    )
    reacquired = False
    identity_check = bool(
        need_redetect
        and (target_cls in ROAD_VEHICLE_CLASSES or target_cls == "motion")
    )
    coast = box if ok else last
    re: Box | None = None
    if need_redetect and yolo_model is not None and target_cls != "motion":
        # Once a road vehicle is semantically known, periodically scan the
        # whole frame. A car hidden by a tree for one second can move outside
        # even an 8x local ROI while CSRT still reports the tree as "ok".
        reacquire_expand = 100.0 if target_cls in ROAD_VEHICLE_CLASSES else adapt_expand
        re = try_yolo_reacquire(
            cv2,
            frame,
            coast,
            yolo_model,
            yolo_names,
            roi_expand=reacquire_expand,
            prev_box=last,
            target_cls=target_cls,
        )
    if re is None and need_redetect and target_cls == "motion":
        re = select_reacquire_candidate(
            motion_candidates,
            coast,
            iou_min=0.03,
            max_area_ratio=2.8,
            min_area_ratio=0.35,
        )
    if re is not None and area_jump_ok(
        re,
        coast,
        max_ratio=2.8 if target_cls == "motion" else AREA_JUMP_MAX,
        min_ratio=0.35 if target_cls == "motion" else AREA_JUMP_MIN,
    ):
        reacquired = True
        box = re
        ok = True
        conf = re.conf
        # Re-init tracker on the fresh detector/motion observation.
        xywh = norm_to_xywh_px(box.x, box.y, box.w, box.h, fw, fh, True)
        # Motion observations correct the displayed center on every frame. A
        # full CSRT reconstruction remains periodic to avoid needless stalls.
        should_reinit = target_cls != "motion" or update_n % TRACK_REDETECT_EVERY == 0
        if should_reinit:
            new_tracker, new_algo = create_cv_tracker(cv2, algo if algo != "TMPL" else None)
            if new_tracker is not None:
                try:
                    ok_ri = new_tracker.init(frame, xywh)
                    if ok_ri is not False:
                        tracker = new_tracker
                        algo = new_algo
                except Exception:  # noqa: BLE001
                    pass
        tmpl, tmpl_wh = init_template_tracker(cv2, frame, xywh)

    # CSRT can report success on a similar patch of foliage/sky after the real
    # vehicle leaves. For semantic road targets, only YOLO may move the public
    # box far from the last confirmed observation. During a short occlusion we
    # hold that confirmed box instead of displaying tracker drift as a target.
    if target_cls in ROAD_VEHICLE_CLASSES and semantic_box is not None and not reacquired:
        box = semantic_box
        box.conf = max(0.05, min(box.conf, 0.5))

    now = time.time()
    with TRACK.lock:
        TRACK.frame_w = fw
        TRACK.frame_h = fh
        TRACK.update_n += 1
        TRACK.ts = now
        if prev_ts > 0:
            TRACK.jpeg_hz = 1.0 / max(1e-3, now - prev_ts)
        TRACK._tracker = tracker
        TRACK.algo = algo
        TRACK._template = tmpl
        TRACK._tmpl_size = tmpl_wh
        if reacquired and target_cls in ROAD_VEHICLE_CLASSES:
            TRACK._semantic_box = box
        if identity_check:
            TRACK.semantic_miss_count = 0 if reacquired else semantic_miss_count + 1
        identity_miss_limit = (
            TRACK_LOST_FAILS
            if target_cls == "motion"
            else max(2, TRACK_LOST_FAILS // TRACK_REDETECT_EVERY)
        )
        identity_expired = (
            (target_cls in ROAD_VEHICLE_CLASSES or target_cls == "motion")
            and TRACK.semantic_miss_count >= identity_miss_limit
        )
        if ok and not identity_expired:
            TRACK.box = box
            TRACK.fail_count = 0
            TRACK.lost = False
            TRACK.ok = True
            TRACK.error = None
        else:
            TRACK.fail_count += 1
            TRACK.box = box  # keep last estimate
            if identity_expired:
                TRACK.lost = True
                TRACK.ok = False
                TRACK.error = "moving object not re-detected after occlusion hold"
            elif TRACK.fail_count >= TRACK_LOST_FAILS:
                TRACK.lost = True
                TRACK.ok = False
                TRACK.error = "DEMO track lost"
            else:
                TRACK.ok = True
                TRACK.error = f"low confidence ({TRACK.fail_count}/{TRACK_LOST_FAILS})"

            # Lab-card evidence: EST jump ratio
            TRACK.area_growth = float(growth)
            TRACK.roi_expand = float(adapt_expand)
            # EST jump from detect-loop range if available
            with STATE.lock:
                cur_r = STATE.range_m
                prev_r = TRACK.prev_range_m
            if cur_r is not None and prev_r is not None and prev_r > 1e-3:
                TRACK.est_jump_ratio = float(cur_r) / float(prev_r)
            else:
                TRACK.est_jump_ratio = None
            if cur_r is not None:
                TRACK.prev_range_m = float(cur_r)

    with TRACK.lock:
        _box = TRACK.box
        _fw = TRACK.frame_w
        _fh = TRACK.frame_h
    update_track_est(_box, _fw, _fh)
    return TRACK.snapshot()


def track_stop(body: dict[str, Any] | None = None) -> dict[str, Any]:
    global EST_FILTER, _TRACK_MOTION_PREV_GRAY
    body = body or {}
    raw_owner = body.get("client_id")
    owner_id = str(raw_owner).strip()[:128] if raw_owner else None
    with TRACK.lock:
        if TRACK.owner_id and owner_id and owner_id != TRACK.owner_id:
            return {
                "ok": True,
                "active": TRACK.active,
                "lost": TRACK.lost,
                "box": None if TRACK.box is None else TRACK.box.as_dict(),
                "label": "DEMO track",
                "ignored": True,
                "error": "stop ignored: track belongs to another GCS tab",
            }
    EST_FILTER = EstFilterState()
    _TRACK_MOTION_PREV_GRAY = None
    with TRACK.lock:
        TRACK.active = False
        TRACK.lost = False
        TRACK.ok = False
        TRACK.box = None
        TRACK._tracker = None
        TRACK._template = None
        TRACK.error = None
        TRACK.fail_count = 0
        TRACK.algo = "none"
        TRACK.target_cls = None
        TRACK.owner_id = None
        TRACK.semantic_miss_count = 0
        TRACK._semantic_box = None
    return {"ok": True, "active": False, "lost": False, "label": "DEMO track", "box": None}



def resolve_weights(explicit: str | None) -> Path | None:
    if explicit:
        p = Path(explicit).expanduser()
        return p if p.is_file() else None
    for p in DEFAULT_WEIGHTS:
        if p.is_file():
            return p
    return None


def list_camera_indices(cv2: Any, max_probe: int = 8) -> list[int]:
    found: list[int] = []
    for i in range(max_probe):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            found.append(i)
        cap.release()
    return found


def open_camera(cv2: Any, index: int | None, max_probe: int) -> tuple[Any, int, str]:
    """Open preferred camera. Name matching is best-effort (AVFoundation)."""
    if index is not None:
        cap = cv2.VideoCapture(index)
        if not cap.isOpened():
            raise RuntimeError(f"Failed to open camera index {index}")
        return cap, index, f"index={index}"

    # Prefer index 0 on Mac when GoPro Webcam is the default virtual cam;
    # also probe a few indices and keep the first that opens.
    indices = list_camera_indices(cv2, max_probe)
    if not indices:
        raise RuntimeError(
            "No camera opened. Start GoPro Webcam app / put GoPro in Webcam Mode."
        )

    # Try to pick a preferred label via CAP_PROP backend strings when available.
    preferred: int | None = None
    labels: dict[int, str] = {}
    for i in indices:
        # Re-open briefly to read size; device names are OS-specific.
        cap = cv2.VideoCapture(i)
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH) or 0)
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT) or 0)
        lab = f"index={i} {w}x{h}"
        labels[i] = lab
        if PREFERRED_CAM_RE.search(lab) and preferred is None:
            preferred = i
        cap.release()

    pick = preferred if preferred is not None else indices[0]
    # Env override for GoPro-friendly default.
    env_idx = os.environ.get("NOSE_EO_CAMERA_INDEX")
    if env_idx is not None and env_idx.isdigit():
        pick = int(env_idx)

    cap = cv2.VideoCapture(pick)
    if not cap.isOpened():
        raise RuntimeError(f"Failed to open camera index {pick}")
    label = labels.get(pick, f"index={pick}")
    # Soft preference note for operators.
    if "GoPro" not in label:
        label = f"{label} (prefer GoPro Webcam if available)"
    return cap, pick, label


def estimate_range_m(box_h_px: float, frame_w: int) -> float:
    """Pinhole DEMO range from assumed target length and HFOV."""
    hfov = math.radians(HFOV_DEG)
    focal_px = (frame_w / 2.0) / math.tan(hfov / 2.0)
    return (TARGET_LENGTH_M * focal_px) / max(float(box_h_px), 1.0)


def motion_fallback(
    cv2: Any,
    frame: Any,
    prev_gray: Any | None,
    *,
    stream_id: str = "default",
) -> tuple[list[Box], Any]:
    """Class-agnostic moving contours using background + frame difference.

    MOG2 removes static scenery over time; intersecting it with immediate frame
    difference rejects exposure changes and most leaf shimmer.
    """
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (11, 11), 0)
    shape = tuple(int(v) for v in gray.shape)
    shape_changed = prev_gray is not None and tuple(prev_gray.shape) != shape
    # Browser tabs may post different resolutions. OpenCV absdiff requires
    # identical shapes, and MOG2 state also belongs to one frame geometry.
    # Isolate each tab/stream and warm up again after any resolution change.
    with _MOTION_LOCK:
        cached = _MOTION_BG.get(stream_id)
        if cached is None or cached[0] != shape:
            background = cv2.createBackgroundSubtractorMOG2(
                history=160,
                varThreshold=28,
                detectShadows=False,
            )
            _MOTION_BG[stream_id] = (shape, background)
        else:
            background = cached[1]
        foreground = background.apply(frame, learningRate=0.012)
    if prev_gray is None or shape_changed:
        return [], gray
    delta = cv2.absdiff(prev_gray, gray)
    _, immediate = cv2.threshold(delta, 18, 255, cv2.THRESH_BINARY)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    # Expand only the temporal gate, then keep the current foreground pixels.
    # This yields a box around the present object instead of the union of its
    # old and new edge positions.
    immediate_gate = cv2.dilate(immediate, kernel, iterations=2)
    mask = cv2.bitwise_and(foreground, immediate_gate)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)
    mask = cv2.dilate(mask, kernel, iterations=1)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    flow_samples: list[tuple[float, float, float, float]] = []
    try:
        corners = cv2.goodFeaturesToTrack(
            prev_gray,
            maxCorners=280,
            qualityLevel=0.01,
            minDistance=5,
            blockSize=5,
        )
        if corners is not None:
            moved, status, _ = cv2.calcOpticalFlowPyrLK(
                prev_gray,
                gray,
                corners,
                None,
                winSize=(21, 21),
                maxLevel=3,
                criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 20, 0.03),
            )
            if moved is not None and status is not None:
                for old, new, ok in zip(corners.reshape(-1, 2), moved.reshape(-1, 2), status.reshape(-1)):
                    if int(ok) != 1:
                        continue
                    ox, oy = float(old[0]), float(old[1])
                    nx, ny = float(new[0]), float(new[1])
                    flow_samples.append((ox, oy, nx - ox, ny - oy))
    except Exception:  # noqa: BLE001
        # Contour-only fallback remains available on reduced OpenCV builds.
        flow_samples = []
    h, w = frame.shape[:2]
    frame_area = float(w * h)
    candidates: list[Box] = []
    for c in contours:
        x, y, bw, bh = cv2.boundingRect(c)
        area = float(bw * bh)
        contour_area = float(cv2.contourArea(c))
        if area < frame_area * 0.00045 or area > frame_area * 0.35:
            continue
        # Border-connected regions are usually exposure/cloud/background
        # changes. Wait until a real object is fully inside the frame.
        if x <= 2 or y <= 2 or x + bw >= w - 2 or y + bh >= h - 2:
            continue
        if bw < 10 or bh < 10:
            continue
        aspect = bw / max(float(bh), 1.0)
        if aspect < 0.25 or aspect > 4.0:
            continue
        fill = contour_area / max(area, 1.0)
        if fill < 0.12:
            continue
        # Require locally coherent feature translation when the ROI contains
        # enough trackable detail. This rejects most leaf/curtain deformation
        # without penalizing a tiny, low-texture distant object.
        pad_x = max(4.0, bw * 0.12)
        pad_y = max(4.0, bh * 0.12)
        local_vectors = [
            (dx, dy)
            for px, py, dx, dy in flow_samples
            if x - pad_x <= px <= x + bw + pad_x
            and y - pad_y <= py <= y + bh + pad_y
        ]
        flow_speed = 0.0
        flow_coherence = 0.0
        if len(local_vectors) >= 4:
            flow_speed, flow_coherence = motion_vector_coherence(local_vectors)
            if flow_speed < 0.7 or flow_coherence < 0.58:
                continue
        candidates.append(
            Box(
                x=x / w,
                y=y / h,
                w=bw / w,
                h=bh / h,
                cls="motion",
                conf=min(
                    0.82,
                    0.34
                    + 4.0 * area / frame_area
                    + 0.15 * fill
                    + (0.12 * flow_coherence if local_vectors else 0.0),
                ),
            )
        )
    candidates.sort(key=lambda box: (box.conf, box.w * box.h), reverse=True)
    return candidates[:8], gray


def pick_target_box(boxes: list[Box]) -> Box | None:
    if not boxes:
        return None
    road = [
        b
        for b in boxes
        if b.cls in ROAD_VEHICLE_CLASSES and b.conf >= MIN_ROAD_AUTO_CONF
    ]
    semantic = [
        b
        for b in boxes
        if b.cls in CANDIDATE_CLASSES
        and b.cls != "motion"
        and b.conf >= MIN_OTHER_AUTO_CONF
    ]
    motion = [b for b in boxes if b.cls == "motion"]
    # Known moving road vehicles keep first priority because they also have a
    # calibrated width. Otherwise prefer actual motion over a static semantic
    # detection: auto mode is explicitly a moving-object detector.
    pool = road or motion or semantic
    if not pool:
        return None
    # Prefer highest confidence, then largest area.
    return max(pool, key=lambda b: (b.conf, b.w * b.h))


class DetectorWorker:
    def __init__(
        self,
        camera_index: int | None,
        weights: Path | None,
        conf: float,
        max_probe: int,
        jpeg_quality: int,
        skip_camera: bool = False,
    ) -> None:
        self.camera_index = camera_index
        self.weights = weights
        self.conf = conf
        self.max_probe = max_probe
        self.jpeg_quality = jpeg_quality
        self.skip_camera = skip_camera
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._run, name="nose-eo-detect", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)

    def _run(self) -> None:
        try:
            import cv2  # type: ignore
        except ImportError:
            with STATE.lock:
                STATE.ok = False
                STATE.error = (
                    "OpenCV (cv2) missing. pip install opencv-python "
                    "(Mac: brew install opencv OR pip install opencv-python)"
                )
            return

        model = None
        names: dict[int, str] = {}
        if self.weights is not None:
            try:
                from ultralytics import YOLO  # type: ignore

                model = YOLO(str(self.weights))
                names = getattr(model, "names", {}) or {}
                _YOLO_RUNTIME["model"] = model
                _YOLO_RUNTIME["names"] = names
                _YOLO_RUNTIME["conf"] = float(self.conf)
                _YOLO_RUNTIME["detector"] = f"yolov8n:{self.weights.name}"
                with STATE.lock:
                    STATE.detector = f"yolov8n:{self.weights.name}"
            except Exception as exc:  # noqa: BLE001 — demo fallback
                with STATE.lock:
                    STATE.detector = "motion"
                    STATE.error = f"YOLO load failed ({exc}); using motion fallback"
                model = None
        else:
            _YOLO_RUNTIME["model"] = None
            _YOLO_RUNTIME["detector"] = "motion"
            with STATE.lock:
                STATE.detector = "motion"
                STATE.error = "yolov8n.pt not found; using motion/contour fallback"

        if self.skip_camera:
            with STATE.lock:
                STATE.ok = False
                STATE.detector = STATE.detector or "none"
                STATE.error = (
                    "Camera skipped (--no-camera). "
                    "POST /detect/frame (raw jpeg) or /track/* with jpeg_b64"
                )
                STATE.camera_label = "browser-frames"
            # Idle loop so thread stays alive for clean shutdown; tracking is request-driven.
            while not self._stop.is_set():
                time.sleep(0.5)
            return

        try:
            cap, idx, label = open_camera(cv2, self.camera_index, self.max_probe)
        except Exception as exc:  # noqa: BLE001
            with STATE.lock:
                STATE.ok = False
                STATE.error = (
                    f"{exc} — tracking still works if GCS POSTs jpeg_b64 to /track/*"
                )
            return

        with STATE.lock:
            STATE.camera_index = idx
            STATE.camera_label = label
            STATE.error = None if model is not None else STATE.error

        prev_gray = None
        prev_range: float | None = None
        fps_t0 = time.time()
        fps_n = 0
        fps = 0.0

        while not self._stop.is_set():
            ok, frame = cap.read()
            if not ok or frame is None:
                time.sleep(0.05)
                continue

            h, w = frame.shape[:2]
            boxes: list[Box] = []

            if model is not None:
                try:
                    results = model.predict(frame, conf=self.conf, verbose=False)
                    if results:
                        r0 = results[0]
                        if r0.boxes is not None and len(r0.boxes):
                            xyxy = r0.boxes.xyxy.cpu().numpy()
                            confs = r0.boxes.conf.cpu().numpy()
                            clss = r0.boxes.cls.cpu().numpy().astype(int)
                            for (x1, y1, x2, y2), cf, ci in zip(xyxy, confs, clss):
                                cls_name = str(names.get(int(ci), str(ci)))
                                bw = max(0.0, float(x2 - x1))
                                bh = max(0.0, float(y2 - y1))
                                boxes.append(
                                    Box(
                                        x=float(x1) / w,
                                        y=float(y1) / h,
                                        w=bw / w,
                                        h=bh / h,
                                        cls=cls_name,
                                        conf=float(cf),
                                    )
                                )
                except Exception as exc:  # noqa: BLE001
                    boxes, prev_gray = motion_fallback(cv2, frame, prev_gray)
                    with STATE.lock:
                        STATE.detector = "motion"
                        STATE.error = f"YOLO infer failed ({exc}); motion fallback"
            else:
                boxes, prev_gray = motion_fallback(cv2, frame, prev_gray)

            target = pick_target_box(boxes)
            range_m: float | None = None
            closing = False
            if target is not None:
                box_h_px = target.h * h
                range_m = estimate_range_m(box_h_px, w)
                if prev_range is not None and range_m < prev_range - CLOSING_EPS_M:
                    closing = True
                prev_range = range_m
            else:
                prev_range = None

            # Annotate JPEG lightly for /mjpeg consumers.
            draw = frame.copy()
            if target is not None:
                x1 = int(target.x * w)
                y1 = int(target.y * h)
                x2 = int((target.x + target.w) * w)
                y2 = int((target.y + target.h) * h)
                cv2.rectangle(draw, (x1, y1), (x2, y2), (90, 200, 255), 2)
                tag = f"TARGET (demo) {target.conf:.2f}"
                if range_m is not None:
                    tag += f"  {range_m:.0f}m"
                    if closing:
                        tag += " CLOSING"
                cv2.putText(
                    draw,
                    tag,
                    (x1, max(16, y1 - 8)),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.55,
                    (90, 200, 255),
                    1,
                    cv2.LINE_AA,
                )

            ok_jpg, buf = cv2.imencode(
                ".jpg",
                draw,
                [int(cv2.IMWRITE_JPEG_QUALITY), self.jpeg_quality],
            )
            jpeg = buf.tobytes() if ok_jpg else None

            fps_n += 1
            now = time.time()
            if now - fps_t0 >= 1.0:
                fps = fps_n / (now - fps_t0)
                fps_t0 = now
                fps_n = 0

            with STATE.lock:
                STATE.ok = True
                STATE.boxes = boxes
                STATE.range_m = range_m
                STATE.closing = closing
                STATE.fps = fps
                STATE.frame_w = w
                STATE.frame_h = h
                STATE.ts = now
                STATE.jpeg = jpeg
                STATE.prev_range_m = prev_range
                if model is not None and STATE.detector.startswith("yolo"):
                    STATE.error = None

            # Pace a bit if inference is very fast.
            time.sleep(0.001)

        cap.release()


def build_app(worker: DetectorWorker) -> Any:
    from fastapi import FastAPI, Request, WebSocket, WebSocketDisconnect
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import JSONResponse, StreamingResponse

    app = FastAPI(title="Nose EO Demo Sidecar", version="0.2.0")
    app.add_middleware(
        CORSMiddleware,
        allow_origins=[
            "http://127.0.0.1:5173",
            "http://localhost:5173",
            "http://127.0.0.1:4173",
            "http://localhost:4173",
        ],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.get("/health")
    def health() -> dict[str, Any]:
        snap = STATE.snapshot()
        tr = TRACK.snapshot()
        # With --no-camera the process is still healthy for browser→/track accompany.
        healthy = bool(snap["ok"] or tr["active"] or worker.skip_camera)
        return {
            "ok": healthy,
            "detector": snap["detector"],
            "camera_label": snap["camera_label"] or ("browser-jpeg" if worker.skip_camera else ""),
            "camera_index": snap["camera_index"],
            "fps": snap["fps"],
            "error": snap["error"],
            "demo": True,
            "track_ready": True,
            "track": {
                "active": tr["active"],
                "lost": tr["lost"],
                "algo": tr["algo"],
            },
            "note": "Geometric DEMO EST (profiles DEMO_GOPRO≠LOCK_CAM_A_40MM), not radar. "
            "Visual accompany via /track/* (browser jpeg preferred).",
        }

    @app.get("/detect")
    def detect() -> JSONResponse:
        return JSONResponse(STATE.snapshot())

    @app.post("/detect/frame")
    async def detect_frame(request: FastAPIRequest) -> JSONResponse:
        """Mac GCS posts raw image/jpeg; headers X-Target-Size-M, X-HFOV-Deg.

        EST uses active calib profile. LOCK_CAM_A_40MM never inherits GoPro ~120°
        HFOV — profile hfov preferred; wide headers ignored (see resolve_frame_hfov).
        """
        body = await request.body()
        if not body:
            return JSONResponse(
                {
                    "ok": False,
                    "error": "empty body — expect raw image/jpeg",
                    "label": "TARGET (demo)",
                    "demo": True,
                    "boxes": [],
                    "range_m": None,
                    "closing": False,
                    "range_status": "none",
                    "jpeg_hz": None,
                    "est_jump_ratio": None,
                },
                status_code=400,
            )

        def _hdr_float(name: str) -> float | None:
            raw = request.headers.get(name)
            if raw is None or raw == "":
                return None
            try:
                return float(raw)
            except ValueError:
                return None

        header_hfov = _hdr_float("X-HFOV-Deg")
        header_size = _hdr_float("X-Target-Size-M")

        try:
            cv2 = _import_cv2()
        except Exception as exc:  # noqa: BLE001
            return JSONResponse(
                {"ok": False, "error": f"OpenCV missing: {exc}", "label": "TARGET (demo)", "demo": True},
                status_code=500,
            )

        import numpy as np

        arr = np.frombuffer(body, dtype=np.uint8)
        frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        if frame is None:
            return JSONResponse(
                {
                    "ok": False,
                    "error": "failed to decode image/jpeg",
                    "label": "TARGET (demo)",
                    "demo": True,
                    "boxes": [],
                    "range_m": None,
                    "closing": False,
                    "range_status": "none",
                    "jpeg_hz": None,
                    "est_jump_ratio": None,
                },
                status_code=400,
            )

        snap = process_posted_detect_frame(
            cv2,
            frame,
            header_hfov=header_hfov,
            header_size=header_size,
            stream_id=f"posted:{request.headers.get('X-Client-Id', 'legacy')[:128]}",
        )
        return JSONResponse(snap)

    @app.post("/dataset/negative/frame")
    async def dataset_negative_frame(request: FastAPIRequest) -> JSONResponse:
        """Store one operator-confirmed false cue for offline evaluation.

        This route is intentionally explicit and single-frame only. It never
        records continuously and never changes detector or mission state.
        """
        body = await request.body()
        if not body:
            return JSONResponse({"ok": False, "error": "empty JPEG body"}, status_code=400)
        if len(body) > MAX_NEGATIVE_JPEG_BYTES:
            return JSONResponse({"ok": False, "error": "JPEG body too large"}, status_code=413)
        try:
            cv2 = _import_cv2()
            frame = decode_jpeg_bytes(cv2, body)
            height, width = frame.shape[:2]
            cue_conf_raw = request.headers.get("X-Cue-Conf", "")
            try:
                cue_conf = float(cue_conf_raw) if cue_conf_raw else None
            except ValueError:
                cue_conf = None
            result = save_negative_sample(
                body,
                NEGATIVE_INBOX,
                reason=request.headers.get("X-Negative-Reason", "false_cue"),
                source=request.headers.get("X-Capture-Source", "nose_eo_browser"),
                width=int(width),
                height=int(height),
                cue={
                    "class": request.headers.get("X-Cue-Class", "")[:64],
                    "confidence": cue_conf,
                },
                session_id=request.headers.get("X-Capture-Session", ""),
                frame_index=(
                    int(request.headers["X-Capture-Index"])
                    if request.headers.get("X-Capture-Index", "").isdigit()
                    else None
                ),
            )
        except ValueError as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        except Exception as exc:  # noqa: BLE001
            return JSONResponse(
                {"ok": False, "error": f"negative capture failed: {exc}"},
                status_code=500,
            )
        return JSONResponse(result)

    @app.get("/dataset/negative/status")
    def dataset_negative_status() -> JSONResponse:
        count = negative_sample_count(NEGATIVE_INBOX)
        minimum = 50
        return JSONResponse(
            {
                "ok": True,
                "count": count,
                "minimum": minimum,
                "ready": count >= minimum,
            }
        )

    def mjpeg_gen():
        boundary = b"--frame"
        while True:
            with STATE.lock:
                jpeg = STATE.jpeg
            if jpeg:
                yield boundary + b"\r\nContent-Type: image/jpeg\r\n\r\n" + jpeg + b"\r\n"
            time.sleep(0.04)

    @app.get("/mjpeg")
    def mjpeg() -> StreamingResponse:
        return StreamingResponse(
            mjpeg_gen(),
            media_type="multipart/x-mixed-replace; boundary=frame",
        )

    @app.get("/track")
    def track_get() -> JSONResponse:
        return JSONResponse(TRACK.snapshot())

    @app.post("/track/init")
    async def track_init_route(request: FastAPIRequest) -> JSONResponse:
        body: dict[str, Any] = {}
        try:
            body = await request.json()
        except Exception:  # noqa: BLE001
            body = {}
        if not isinstance(body, dict):
            body = {}
        return JSONResponse(track_init(body))

    @app.post("/track/init/frame")
    async def track_init_frame_route(request: FastAPIRequest) -> JSONResponse:
        """Initialize a normalized ROI from raw JPEG bytes."""
        try:
            cv2 = _import_cv2()
            frame = decode_jpeg_bytes(cv2, await request.body())
            body: dict[str, Any] = {
                "x": float(request.headers["X-ROI-X"]),
                "y": float(request.headers["X-ROI-Y"]),
                "w": float(request.headers["X-ROI-W"]),
                "h": float(request.headers["X-ROI-H"]),
                "normalized": True,
                "client_id": request.headers.get("X-Client-Id", "").strip()[:128],
            }
            target_cls = request.headers.get("X-Target-Class", "").strip()
            if target_cls:
                body["target_cls"] = target_cls
        except Exception as exc:  # noqa: BLE001
            return JSONResponse(
                {
                    "ok": False,
                    "active": False,
                    "lost": True,
                    "box": None,
                    "error": f"Invalid raw track init: {exc}",
                },
                status_code=400,
            )
        return JSONResponse(track_init(body, frame_override=frame))

    @app.post("/track/update")
    async def track_update_route(request: FastAPIRequest) -> JSONResponse:
        body: dict[str, Any] = {}
        try:
            body = await request.json()
        except Exception:  # noqa: BLE001
            body = {}
        if not isinstance(body, dict):
            body = {}
        return JSONResponse(track_update(body))

    @app.post("/track/update/frame")
    async def track_update_frame_route(request: FastAPIRequest) -> JSONResponse:
        """Step tracker from raw JPEG bytes without Base64/JSON expansion."""
        try:
            cv2 = _import_cv2()
            frame = decode_jpeg_bytes(cv2, await request.body())
        except Exception as exc:  # noqa: BLE001
            return JSONResponse(
                {
                    "ok": False,
                    "active": False,
                    "lost": False,
                    "box": None,
                    "error": str(exc),
                },
                status_code=400,
            )
        client_id = request.headers.get("X-Client-Id", "").strip()[:128]
        return JSONResponse(
            track_update({"client_id": client_id}, frame_override=frame)
        )

    @app.get("/track/update")
    def track_update_get() -> JSONResponse:
        """Step tracker using last camera JPEG (no browser frame)."""
        return JSONResponse(track_update({}))

    @app.post("/track/stop")
    async def track_stop_route(request: FastAPIRequest) -> JSONResponse:
        body: dict[str, Any] = {}
        try:
            body = await request.json()
        except Exception:  # noqa: BLE001
            body = {}
        if not isinstance(body, dict):
            body = {}
        return JSONResponse(track_stop(body))


    @app.get("/config")
    def config_get() -> JSONResponse:
        with _KNOBS_LOCK:
            payload = KNOBS.as_public_dict(PROFILES)
        payload["profiles"] = {
            pid: {
                "hfov_deg": float(meta.get("hfov_deg", 0)),
                "label": meta.get("label"),
                "lock_eligible": bool(meta.get("lock_eligible", False)),
                "notes": meta.get("notes", ""),
            }
            for pid, meta in PROFILES.items()
        }
        payload["lost_hold_s"] = LOST_HOLD_S
        return JSONResponse(payload)

    @app.post("/config")
    async def config_post(request: FastAPIRequest) -> JSONResponse:
        body: dict[str, Any] = {}
        try:
            body = await request.json()
        except Exception:  # noqa: BLE001
            body = {}
        if not isinstance(body, dict):
            body = {}
        try:
            payload = apply_knobs_patch(body)
        except ValueError as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        payload["ok"] = True
        return JSONResponse(payload)

    @app.post("/calib/hfov")
    async def calib_hfov_route(request: FastAPIRequest) -> JSONResponse:
        """Calibrate HFOV at known range BEFORE fast runs (Known width + range).

        Body: {known_range_m, box_w_norm|box_w_px, frame_w?}
        Uses current profile's known_width_m unless known_width_m provided.
        """
        body: dict[str, Any] = {}
        try:
            body = await request.json()
        except Exception:  # noqa: BLE001
            body = {}
        if not isinstance(body, dict):
            body = {}
        try:
            known_range = float(body["known_range_m"])
            with _KNOBS_LOCK:
                known_w = float(body.get("known_width_m", KNOBS.known_width_m))
            if "box_w_px" in body:
                box_w_px = float(body["box_w_px"])
                frame_w = int(body.get("frame_w") or TRACK.frame_w or 0)
            else:
                box_w_norm = float(body["box_w_norm"])
                frame_w = int(body.get("frame_w") or TRACK.frame_w or 0)
                box_w_px = box_w_norm * float(frame_w)
            if frame_w <= 0:
                raise ValueError("frame_w required (or init track first)")
            hfov = calibrate_hfov_deg(known_w, known_range, box_w_px, frame_w)
            with _KNOBS_LOCK:
                KNOBS.hfov_deg = float(hfov)
                KNOBS.known_width_m = float(known_w)
                payload = KNOBS.as_public_dict(PROFILES)
            payload["ok"] = True
            payload["calibrated_hfov_deg"] = round(hfov, 3)
            payload["note"] = (
                f"HFOV calibrated on profile {payload['profile_id']} — "
                "do NOT copy DEMO_GOPRO FoV onto LOCK_CAM_A_40MM"
            )
            return JSONResponse(payload)
        except (KeyError, TypeError, ValueError) as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)

    @app.websocket("/ws")
    async def ws_detect(ws: WebSocket) -> None:
        await ws.accept()
        try:
            while True:
                await ws.send_json(STATE.snapshot())
                await asyncio.sleep(0.1)
        except WebSocketDisconnect:
            return
        except Exception:  # noqa: BLE001
            try:
                await ws.close()
            except Exception:  # noqa: BLE001
                pass

    @app.on_event("startup")
    def _startup() -> None:
        worker.start()

    @app.on_event("shutdown")
    def _shutdown() -> None:
        worker.stop()

    return app


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--camera-index", type=int, default=None)
    parser.add_argument("--weights", type=str, default=None, help="Path to yolov8n.pt")
    parser.add_argument("--conf", type=float, default=0.20)
    parser.add_argument("--max-probe", type=int, default=8)
    parser.add_argument("--jpeg-quality", type=int, default=72)
    parser.add_argument(
        "--no-camera",
        action="store_true",
        help="Skip OpenCV camera open — track via browser jpeg_b64 only (Mac TCC friendly)",
    )
    args = parser.parse_args()

    weights = resolve_weights(args.weights)
    if weights is None:
        print(
            "WARN: yolov8n.pt not found (checked vision/yolov8n.pt). "
            "Will use motion/contour fallback. "
            "Place weights at vision/yolov8n.pt or pass --weights.",
            file=sys.stderr,
        )
    else:
        print(f"Using weights: {weights}")

    try:
        import uvicorn
    except ImportError:
        print(
            "uvicorn/fastapi required:\n"
            "  pip install ultralytics opencv-python fastapi uvicorn",
            file=sys.stderr,
        )
        return 2

    worker = DetectorWorker(
        camera_index=args.camera_index,
        weights=weights,
        conf=args.conf,
        max_probe=args.max_probe,
        jpeg_quality=args.jpeg_quality,
        skip_camera=bool(args.no_camera),
    )
    app = build_app(worker)
    print(
        f"Nose EO demo sidecar on http://{args.host}:{args.port}  "
        f"(CORS → localhost:5173)  DEMO range L={TARGET_LENGTH_M}m HFOV={HFOV_DEG}°"
        f"{'  [no-camera · browser jpeg track]' if args.no_camera else ''}"
    )
    uvicorn.run(app, host=args.host, port=args.port, log_level="info")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
