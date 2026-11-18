#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Capture the running task manager with real /proc data in isolated UKUI sessions.
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${1:-${PROJECT_DIR}/build-openkylin/kylin-monitor}"
OUTPUT="${2:-${PROJECT_DIR}/docs/previews/task-manager}"
for theme in light dark; do
    mkdir -p "$OUTPUT/$theme"
    bash "$PROJECT_DIR/scripts/test-peony.sh" bash -c '
        gsettings set org.ukui.style style-name "ukui-$1"
        exec "$2" --processes --capture-dir "$3"
    ' _ "$theme" "$BINARY" "$OUTPUT/$theme"
done
