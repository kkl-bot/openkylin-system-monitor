#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "${PROJECT_DIR}/build-openkylin/CPackConfig.cmake" ]]; then
    bash "${PROJECT_DIR}/scripts/build.sh"
fi
# Native Linux staging preserves 0755 / 0644 permissions; WSL DrvFS may report 0777.
PACKAGE_TEMP="$(mktemp -d /tmp/kylin-monitor-package.XXXXXX)"
trap 'case "$PACKAGE_TEMP" in /tmp/kylin-monitor-package.*) rm -rf -- "$PACKAGE_TEMP" ;; esac' EXIT
cpack --config "${PROJECT_DIR}/build-openkylin/CPackConfig.cmake" -B "$PACKAGE_TEMP"
mkdir -p "${PROJECT_DIR}/dist"
cp "$PACKAGE_TEMP"/*.deb "${PROJECT_DIR}/dist/"
printf 'Package saved to: %s\n' "${PROJECT_DIR}/dist"
