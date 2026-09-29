"""Nose EO DEMO monocular EST + calib profiles (pure helpers).

DEMO ONLY — not radar/LiDAR. Profiles from vision/configs/calib_profiles.yaml.
Hard rule: never apply a wide-camera HFOV to a narrow TELE profile.
"""
from __future__ import annotations

import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
PROFILES_PATH = ROOT / "vision" / "configs" / "calib_profiles.yaml"

# Fallbacks if YAML missing (must match calib_profiles.yaml)
_FALLBACK_PROFILES: dict[str, dict[str, Any]] = {
    "DEMO_GOPRO": {
        "lock_eligible": False,
        "label": "DEMO",
        "optic": "gopro_class",
        "hfov_deg": 120.0,
        "notes": "Lab Mac Nose EO only. Never use for Cam A EST defaults.",
    },
    "LOCK_CAM_A_40MM": {
        "lock_eligible": True,
        "label": "LOCK",
        "optic": "basler_a2A1920-168mgm_40mm",
        "hfov_deg": 10.0,
        "notes": "Flight Lock path. Separate calib UI profile from DEMO_GOPRO.",
    },
    "ELP_AR0234_SEARCH_100DEG": {
        "lock_eligible": False,
        "calibrated": False,
        "label": "SEARCH",
        "optic": "elp_ar0234_usb3_wide",
        "hfov_deg": 100.0,
        "notes": "Provisional ELP SEARCH profile; calibrate the received unit.",
    },
    "ELP_AR0234_TELE_50MM": {
        "lock_eligible": False,
        "calibrated": False,
        "label": "TELE_TEST",
        "optic": "elp_ar0234_usb3_5_50mm",
        "hfov_deg": 7.0,
        "notes": "Provisional ELP TELE profile; requires FOV/extrinsic calibration.",
    },
}

DEFAULT_PROFILE = "DEMO_GOPRO"
DEFAULT_KNOWN_WIDTH_M = 0.08  # lab bottle; flight uses class/known width
DEFAULT_EMA_ALPHA = 0.35
DEFAULT_EMA_ALPHA_FAST = 0.55  # when closing quickly
DEFAULT_MAX_JUMP_RATIO = 2.0  # reject |R/Rprev| > 2
DEFAULT_MIN_BOX_FRAC = 0.012  # too_small if width frac below this
DEFAULT_ROI_EXPAND = 3.0  # larger search for fast translate
DEFAULT_JPEG_MAX_W = 640
DEFAULT_JPEG_QUALITY = 0.72
DEFAULT_JPEG_HZ = 12.5  # ~80 ms poll
# A brief occlusion (tree/pole/vehicle crossing) must not immediately destroy a
# visual track.  The tracker keeps the last box while the detector attempts
# reacquisition; mission code still decides whether a coast is usable.
LOST_HOLD_S = 2.5


def lost_fails_for_hz(jpeg_hz: float, hold_s: float = LOST_HOLD_S) -> int:
    hz = max(1.0, float(jpeg_hz))
    return max(3, int(round(hold_s * hz)))


def _parse_simple_profiles_yaml(text: str) -> dict[str, dict[str, Any]]:
    """Minimal YAML subset parser for calib_profiles.yaml (no PyYAML required)."""
    profiles: dict[str, dict[str, Any]] = {}
    current: str | None = None
    in_profiles = False
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if re.match(r"^profiles:\s*$", line):
            in_profiles = True
            continue
        if not in_profiles:
            continue
        m_prof = re.match(r"^  ([A-Za-z0-9_]+):\s*$", line)
        if m_prof:
            current = m_prof.group(1)
            profiles[current] = {}
            continue
        if current is None:
            continue
        m_kv = re.match(r"^    ([A-Za-z0-9_]+):\s*(.+)$", line)
        if not m_kv:
            continue
        key, val = m_kv.group(1), m_kv.group(2).strip()
        if val.lower() in ("true", "false"):
            profiles[current][key] = val.lower() == "true"
        else:
            try:
                if "." in val:
                    profiles[current][key] = float(val)
                else:
                    profiles[current][key] = int(val)
            except ValueError:
                profiles[current][key] = val.strip("\"'")
    return profiles


def load_calib_profiles(path: Path | None = None) -> dict[str, dict[str, Any]]:
    p = path or PROFILES_PATH
    if p.is_file():
        try:
            parsed = _parse_simple_profiles_yaml(p.read_text(encoding="utf-8"))
            if parsed:
                # Ensure required keys
                out = dict(_FALLBACK_PROFILES)
                out.update(parsed)
                return out
        except OSError:
            pass
    return dict(_FALLBACK_PROFILES)


def profile_hfov_deg(profile_id: str, profiles: dict[str, dict[str, Any]] | None = None) -> float:
    profs = profiles or load_calib_profiles()
    pid = profile_id if profile_id in profs else DEFAULT_PROFILE
    return float(profs[pid].get("hfov_deg", 120.0))


def profile_lock_eligible(profile_id: str, profiles: dict[str, dict[str, Any]] | None = None) -> bool:
    profs = profiles or load_calib_profiles()
    pid = profile_id if profile_id in profs else DEFAULT_PROFILE
    return bool(profs[pid].get("lock_eligible", False))


def profile_label(profile_id: str, profiles: dict[str, dict[str, Any]] | None = None) -> str:
    profs = profiles or load_calib_profiles()
    pid = profile_id if profile_id in profs else DEFAULT_PROFILE
    return str(profs[pid].get("label", "DEMO"))


@dataclass
class EstKnobs:
    """Tunable EST + track hold knobs (DEMO sidecar + GCS)."""

    profile_id: str = DEFAULT_PROFILE
    known_width_m: float = DEFAULT_KNOWN_WIDTH_M
    hfov_deg: float | None = None  # None → from profile; set after Calibrate FOV
    ema_alpha: float = DEFAULT_EMA_ALPHA
    ema_alpha_fast: float = DEFAULT_EMA_ALPHA_FAST
    max_jump_ratio: float = DEFAULT_MAX_JUMP_RATIO
    min_box_frac: float = DEFAULT_MIN_BOX_FRAC
    roi_expand: float = DEFAULT_ROI_EXPAND
    jpeg_max_w: int = DEFAULT_JPEG_MAX_W
    jpeg_quality: float = DEFAULT_JPEG_QUALITY
    jpeg_hz: float = DEFAULT_JPEG_HZ
    track_lost_fails: int | None = None  # None → derived from jpeg_hz * LOST_HOLD_S
    track_redetect_every: int = 4
    csrt_padding: float = 3.0  # larger search window for fast motion
    closing_eps_m: float = 0.15

    def effective_hfov_deg(self, profiles: dict[str, dict[str, Any]] | None = None) -> float:
        if self.hfov_deg is not None and self.hfov_deg > 0:
            return float(self.hfov_deg)
        return profile_hfov_deg(self.profile_id, profiles)

    def effective_lost_fails(self) -> int:
        if self.track_lost_fails is not None and self.track_lost_fails > 0:
            return int(self.track_lost_fails)
        return lost_fails_for_hz(self.jpeg_hz)

    def as_public_dict(self, profiles: dict[str, dict[str, Any]] | None = None) -> dict[str, Any]:
        profs = profiles or load_calib_profiles()
        pid = self.profile_id if self.profile_id in profs else DEFAULT_PROFILE
        return {
            "profile_id": pid,
            "profile_label": profile_label(pid, profs),
            "lock_eligible": profile_lock_eligible(pid, profs),
            "known_width_m": self.known_width_m,
            "hfov_deg": self.effective_hfov_deg(profs),
            "hfov_override": self.hfov_deg is not None,
            "ema_alpha": self.ema_alpha,
            "ema_alpha_fast": self.ema_alpha_fast,
            "max_jump_ratio": self.max_jump_ratio,
            "min_box_frac": self.min_box_frac,
            "roi_expand": self.roi_expand,
            "jpeg_max_w": self.jpeg_max_w,
            "jpeg_quality": self.jpeg_quality,
            "jpeg_hz": self.jpeg_hz,
            "track_lost_fails": self.effective_lost_fails(),
            "track_redetect_every": self.track_redetect_every,
            "lost_hold_s": LOST_HOLD_S,
            "csrt_padding": self.csrt_padding,
            "closing_eps_m": self.closing_eps_m,
            "demo": True,
            "note": "EST monocular DEMO — SEARCH bias only; not Lock / not radar",
        }


def classify_box_status(
    box_w_norm: float,
    box_h_norm: float,
    x_norm: float,
    y_norm: float,
    min_box_frac: float = DEFAULT_MIN_BOX_FRAC,
) -> str:
    """Return ok | clipped | too_small."""
    if box_w_norm < min_box_frac or box_h_norm < min_box_frac:
        return "too_small"
    # Touching frame edge → clipped
    eps = 0.002
    if x_norm <= eps or y_norm <= eps or (x_norm + box_w_norm) >= 1.0 - eps or (y_norm + box_h_norm) >= 1.0 - eps:
        return "clipped"
    return "ok"


def estimate_range_m(
    box_w_px: float,
    frame_w: int,
    known_width_m: float,
    hfov_deg: float,
) -> float:
    """Pinhole EST from horizontal span + Known width + HFOV."""
    hfov = math.radians(hfov_deg)
    focal_px = (frame_w / 2.0) / math.tan(hfov / 2.0)
    return (known_width_m * focal_px) / max(float(box_w_px), 1.0)


def calibrate_hfov_deg(
    known_width_m: float,
    known_range_m: float,
    box_w_px: float,
    frame_w: int,
) -> float:
    """Solve HFOV from known width + known range + measured box width (px)."""
    if known_range_m <= 0 or known_width_m <= 0 or box_w_px <= 0 or frame_w <= 0:
        raise ValueError("calib needs positive known_width_m, known_range_m, box_w_px, frame_w")
    # range = (W * f) / box_w  →  f = range * box_w / W
    # f = (frame_w/2) / tan(hfov/2)  →  tan(hfov/2) = (frame_w/2) / f
    focal_px = (known_range_m * box_w_px) / known_width_m
    tan_half = (frame_w / 2.0) / max(focal_px, 1e-6)
    tan_half = min(max(tan_half, 1e-6), 0.999999)
    hfov = 2.0 * math.atan(tan_half)
    return math.degrees(hfov)


@dataclass
class EstFilterState:
    prev_raw_m: float | None = None
    prev_filt_m: float | None = None
    closing: bool = False


def apply_est_filter(
    raw_m: float | None,
    state: EstFilterState,
    *,
    range_status: str,
    alpha: float = DEFAULT_EMA_ALPHA,
    alpha_fast: float = DEFAULT_EMA_ALPHA_FAST,
    max_jump_ratio: float = DEFAULT_MAX_JUMP_RATIO,
    closing_eps_m: float = 0.15,
) -> tuple[float | None, str, EstFilterState]:
    """Adaptive EMA + jump reject |R/Rprev|>max_jump_ratio.

    Returns (filtered_range_m, range_status, new_state).
    Status may become jump_rejected when spike rejected (coast previous).
    """
    st = EstFilterState(prev_raw_m=state.prev_raw_m, prev_filt_m=state.prev_filt_m, closing=False)
    if raw_m is None or range_status in ("too_small", "clipped"):
        # Do not update filter on bad boxes — coast
        st.closing = False
        return st.prev_filt_m, range_status, st

    status = range_status
    # Jump reject vs previous filtered (or raw)
    ref = st.prev_filt_m if st.prev_filt_m is not None else st.prev_raw_m
    if ref is not None and ref > 1e-6:
        ratio = raw_m / ref
        if ratio > max_jump_ratio or ratio < (1.0 / max_jump_ratio):
            status = "jump_rejected"
            st.closing = False
            return st.prev_filt_m, status, st

    # Adaptive alpha: faster when clearly closing
    use_alpha = alpha
    if st.prev_filt_m is not None and raw_m < st.prev_filt_m - closing_eps_m:
        use_alpha = alpha_fast

    if st.prev_filt_m is None:
        filt = raw_m
    else:
        filt = use_alpha * raw_m + (1.0 - use_alpha) * st.prev_filt_m

    if st.prev_filt_m is not None and filt < st.prev_filt_m - closing_eps_m:
        st.closing = True
    st.prev_raw_m = raw_m
    st.prev_filt_m = filt
    return filt, status, st


def simulate_closing_bbox_sequence(
    *,
    n: int = 20,
    start_x: float = 0.35,
    start_y: float = 0.40,
    start_w: float = 0.08,
    start_h: float = 0.10,
    dx: float = 0.012,
    grow: float = 1.06,
) -> list[tuple[float, float, float, float]]:
    """Synthetic fast-translate + growing bbox (closing sim) for host tests."""
    boxes = []
    x, y, w, h = start_x, start_y, start_w, start_h
    for _ in range(n):
        boxes.append((x, y, w, h))
        x = min(0.85, x + dx)
        y = min(0.80, max(0.05, y + dx * 0.15))
        w = min(0.45, w * grow)
        h = min(0.50, h * grow)
        # keep in frame
        x = min(x, 1.0 - w)
        y = min(y, 1.0 - h)
    return boxes
