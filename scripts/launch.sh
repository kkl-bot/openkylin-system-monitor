#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-openkylin"
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
fi
# Preserve existing CMake options and rebuild only stale application inputs.
# An executable's existence alone does not mean it matches the current source.
cmake --build "$BUILD_DIR" --target kylin-monitor --parallel 4
exec "${BUILD_DIR}/kylin-monitor" "$@"
