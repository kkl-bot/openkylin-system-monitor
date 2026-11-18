#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
for theme in light dark; do
    bash "$PROJECT_DIR/scripts/test-peony.sh" bash -c '
        gsettings set org.ukui.style style-name "ukui-$1"
        exec "$2/build-openkylin/layout_preview" "$2/docs/previews/window-layout/$1"
    ' _ "$theme" "$PROJECT_DIR"
done
