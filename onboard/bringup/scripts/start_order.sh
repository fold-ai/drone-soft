#!/usr/bin/env bash
# ActProve V1 onboard start order — DO NOT start cameras before PPS ready.
# Owner: System stack. ICD: docs/icd/time_sync.md, docs/bringup/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
ROOT="${ACTPROVE_ROOT:-$REPO_ROOT}"
BUILD="${ACTPROVE_ONBOARD_BUILD:-$ROOT/onboard/build}"
LOG_ROOT="${ACTPROVE_BLACKBOX:-/tmp/actprove_blackbox}"
# On Orin prefer /data/blackbox when writable:
if [[ -d /data/blackbox && -w /data/blackbox ]]; then
  LOG_ROOT="${ACTPROVE_BLACKBOX:-/data/blackbox}"
fi
TELEM2_PORT="${ACTPROVE_TELEM2:-/dev/ttyTHS1}"
TELEM2_BAUD="${ACTPROVE_TELEM2_BAUD:-57600}"
SIMULATE="${ACTPROVE_SIMULATE:-0}"

log() { printf '[bringup %s] %s\n' "$(date '+%H:%M:%S')" "$*"; }

if [[ "$SIMULATE" != "1" ]]; then
  log "FATAL: production backends are incomplete; refusing scaffold bring-up"
  log "Set ACTPROVE_SIMULATE=1 only for an explicit host/demo run"
  exit 1
fi

log "repo=$ROOT build=$BUILD"
log "1/5 time_sync — explicit simulation"
mkdir -p /tmp/actprove
: > /tmp/actprove/pps_ready  # scaffold; replace with time_sync --wait
log "PPS ready gate cleared (scaffold). Cameras MUST NOT start before this."

log "2/5 logging — open NVMe black-box run"
mkdir -p "$LOG_ROOT"
if [[ -x "$BUILD/logger_smoke" ]]; then
  "$BUILD/logger_smoke" --out "$LOG_ROOT" --seconds 1 || true
else
  log "WARN: logger_smoke not built at $BUILD/logger_smoke"
fi

log "3/5 mavlink_bridge — TELEM2 UART @ ${TELEM2_BAUD}"
if [[ -x "$BUILD/mavlink_bridge_smoke" ]]; then
  "$BUILD/mavlink_bridge_smoke" --port "$TELEM2_PORT" --baud "$TELEM2_BAUD" --smoke || true
else
  log "WARN: mavlink_bridge_smoke not built"
fi

log "4/5 safety_gates — mission SM hooks (BOOT)"
log "safety_gates stub: state=BOOT (C++ SM + optional C mission_sm)"

log "5/5 perception hooks — ONLY after PPS ready"
if [[ -f /tmp/actprove/pps_ready ]]; then
  log "PPS gate OK — vision/apps/detect_node may start (not launched here)"
else
  log "FATAL: PPS not ready — refusing camera start"
  exit 1
fi

log "bringup sequence complete"
