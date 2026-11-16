#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bash "${PROJECT_DIR}/scripts/build.sh"
bash "${PROJECT_DIR}/scripts/package.sh"
VERSION="$(sed -n 's/^project(KylinMonitor VERSION \([^ ]*\).*/\1/p' "${PROJECT_DIR}/CMakeLists.txt")"
PACKAGE="${PROJECT_DIR}/dist/kylin-monitor-${VERSION}-linux-amd64.deb"
if [[ "$(id -u)" == 0 ]]; then
    apt-get install -y "$PACKAGE"
else
    sudo apt-get install -y "$PACKAGE"
fi
printf '%s\n' 'Installed. Remove old desktop instances if desired, then log out/in to refresh Peony discovery.'
printf '%s\n' 'Desktop context menu → More plugins → System Monitor → choose a size.'
