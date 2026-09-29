#!/usr/bin/env bash
# Best-effort stop of System stack services (scaffold).
set -euo pipefail
log() { printf '[bringup-stop %s] %s\n' "$(date '+%H:%M:%S')" "$*"; }
for unit in actprove-detect actprove-mavlink actprove-logging actprove-time-sync; do
  if systemctl is-active --quiet "$unit" 2>/dev/null; then
    log "stopping $unit"
    sudo systemctl stop "$unit" || true
  fi
done
rm -f /tmp/actprove/pps_ready
log "done"
