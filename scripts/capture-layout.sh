#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if (( $# > 1 )); then
    printf 'Usage: %s [output-root]\n' "$0" >&2
    exit 2
fi
OUTPUT="${1:-${PROJECT_DIR}/docs/previews/typography}"
for theme in light dark; do
    mkdir -p "$OUTPUT/$theme"
    bash "$PROJECT_DIR/scripts/test-peony.sh" bash -c '
        gsettings set org.ukui.style style-name "ukui-$1"
        exec "$2/build-openkylin/layout_preview" "$3"
    ' _ "$theme" "$PROJECT_DIR" "$OUTPUT/$theme"
done
