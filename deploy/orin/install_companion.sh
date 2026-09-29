#!/usr/bin/env bash
# Install companion + vision systemd units. Run on the Orin as root.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "refusing: install on the Orin (aarch64), not $(uname -m)" >&2
  exit 1
fi
install -d /etc/systemd/system
install -m 0644 "$ROOT/deploy/orin/systemd/actprove-companion.service" /etc/systemd/system/
install -m 0644 "$ROOT/deploy/orin/systemd/actprove-vision.service" /etc/systemd/system/
systemctl daemon-reload
# Logger binds the same IPC socket — do not enable it with companion.
systemctl disable --now actprove-passive-detection-logger.service 2>/dev/null || true
systemctl disable --now actprove-passive-vision.service 2>/dev/null || true
id actprove >/dev/null 2>&1 || useradd --system --home /opt/drone-soft --shell /usr/sbin/nologin actprove
usermod -aG video,dialout actprove || true
systemctl enable actprove-companion.service actprove-vision.service
echo "enabled. start with: systemctl start actprove-companion actprove-vision"
echo "logs: journalctl -u actprove-companion -u actprove-vision -f"
