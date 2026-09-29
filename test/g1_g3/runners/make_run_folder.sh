#!/usr/bin/env bash
set -euo pipefail
GATE=${1:?G1|G2|G3}
ID=${2:-run}
STAMP=$(date +%Y%m%d_%H%M%S)
ROOT=${3:-./logs}
case "$GATE" in
  G1|G2|G3) ;;
  *) echo "invalid gate: $GATE" >&2; exit 2 ;;
esac
if [[ ! "$ID" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "invalid run id: use letters, digits, dot, underscore, or dash" >&2
  exit 2
fi
if [[ "$GATE" != "G1" && -z "${ACTPROVE_FTS_HOLDER:-}" ]]; then
  echo "$GATE requires ACTPROVE_FTS_HOLDER" >&2
  exit 2
fi
DIR="$ROOT/${GATE}_${STAMP}_${ID}"
mkdir -p "$DIR/streams" "$DIR/fc"
python3 - "$GATE" "$ID" "$DIR/manifest.json" <<'PY'
import datetime
import json
import os
import pathlib
import sys

gate, run_id, output = sys.argv[1:]
manifest = {
    "gate": gate,
    "run_id": run_id,
    "started_at": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
    "operator": os.environ.get("ACTPROVE_OPERATOR", os.environ.get("USER", "UNKNOWN")),
    "git_sha": os.environ.get("ACTPROVE_GIT_SHA", "UNKNOWN"),
    "fts_holder": os.environ.get("ACTPROVE_FTS_HOLDER") or None,
    "airspeed_source": os.environ.get("ACTPROVE_AIRSPEED_SOURCE", "na_ground"),
}
pathlib.Path(output).write_text(json.dumps(manifest, indent=2) + "\n")
PY
echo "$DIR"
