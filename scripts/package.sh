#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-openkylin"
if [[ -f "$BUILD_DIR/CPackConfig.cmake" ]]; then
    cmake --build "$BUILD_DIR" --parallel 4
else
    bash "${PROJECT_DIR}/scripts/build.sh"
fi
# Native Linux staging preserves 0755 / 0644 permissions; WSL DrvFS may report 0777.
PACKAGE_TEMP="$(mktemp -d /tmp/kylin-monitor-package.XXXXXX)"
trap 'case "$PACKAGE_TEMP" in /tmp/kylin-monitor-package.*) rm -rf -- "$PACKAGE_TEMP" ;; esac' EXIT
cpack --config "$BUILD_DIR/CPackConfig.cmake" -B "$PACKAGE_TEMP"
mkdir -p "${PROJECT_DIR}/dist"
cp "$PACKAGE_TEMP"/*.deb "${PROJECT_DIR}/dist/"
printf 'Package saved to: %s\n' "${PROJECT_DIR}/dist"
