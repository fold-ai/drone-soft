#!/usr/bin/env bash
# Copy flight source to Orin. Does not copy venv, node_modules, or host build trees.
set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo "usage: $0 user@orin-host [dest=/opt/drone-soft]" >&2
  exit 2
fi
HOST="$1"
DEST="${2:-/opt/drone-soft}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
rsync -az --delete \
  --exclude '.git/' \
  --exclude '.venv*/' \
  --exclude '**/node_modules/' \
  --exclude '**/build/' \
  --exclude 'gcs-ui/dist/' \
  --exclude '**/.DS_Store' \
  "$ROOT/" "$HOST:$DEST/"
echo "copied $ROOT → $HOST:$DEST"
echo "on Orin:"
echo "  cmake -S $DEST/onboard -B $DEST/onboard/build -DCMAKE_BUILD_TYPE=Release && cmake --build $DEST/onboard/build -j\$(nproc)"
echo "  cmake -S $DEST/vision -B $DEST/vision/build -DSOFT_NO_JETPACK=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build $DEST/vision/build -j\$(nproc)"
echo "  python3 $DEST/tools/build_tensorrt_engine.py --onnx $DEST/vision/training/exports/smoke_e2.onnx --engine $DEST/vision/models/engines/yolov8n_fp16.engine"
echo "  sudo $DEST/deploy/orin/install_companion.sh"
