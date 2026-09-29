"""Shared paths for the training system (vision/training/)."""
from __future__ import annotations

from pathlib import Path

TRAINING_ROOT = Path(__file__).resolve().parents[1]
VISION_ROOT = TRAINING_ROOT.parent
DATASETS = TRAINING_ROOT / "datasets"
RAW = DATASETS / "raw"
MANIFESTS = DATASETS / "manifests"
LISTS = DATASETS / "lists"
SMOKE = DATASETS / "smoke"
RUNS = TRAINING_ROOT / "runs"
EXPORTS = TRAINING_ROOT / "exports"
CONFIGS = TRAINING_ROOT / "configs"
INBOX = VISION_ROOT / "data" / "inbox"
VENV_PYTHON = VISION_ROOT / ".venv" / "bin" / "python"

# V1 gated classes only
CLASSES = {
    "shahed_136": 0,
    "geran_2": 1,
    "gerbera": 2,
    "negative": None,
}
CLASS_NAMES = {0: "shahed_136", 1: "geran_2", 2: "gerbera"}
VALID_CLASS_IDS = {0, 1, 2}
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}
