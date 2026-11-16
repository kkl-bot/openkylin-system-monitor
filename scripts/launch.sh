#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -x "${PROJECT_DIR}/build-openkylin/kylin-monitor" ]]; then
    bash "${PROJECT_DIR}/scripts/build.sh"
fi
exec "${PROJECT_DIR}/build-openkylin/kylin-monitor" "$@"
