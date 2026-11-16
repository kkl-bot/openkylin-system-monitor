#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${HOME}/.local"
if [[ ! -x "${PROJECT_DIR}/build-openkylin/kylin-monitor" ]]; then
    bash "${PROJECT_DIR}/scripts/build.sh"
fi
cmake --install "${PROJECT_DIR}/build-openkylin" --prefix "$INSTALL_PREFIX" --component Application
LAUNCHER="${INSTALL_PREFIX}/share/applications/kylin-monitor.desktop"
sed "s|^Exec=.*|Exec=\"${INSTALL_PREFIX}/bin/kylin-monitor\" --detail|" \
    "${PROJECT_DIR}/packaging/kylin-monitor.desktop" > "$LAUNCHER"
if command -v xdg-user-dir >/dev/null 2>&1; then
    DESKTOP_DIR="$(xdg-user-dir DESKTOP)"
    if [[ -d "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$HOME" ]]; then
        cp "$LAUNCHER" "${DESKTOP_DIR}/kylin-monitor.desktop"
        chmod +x "${DESKTOP_DIR}/kylin-monitor.desktop"
    fi
fi
printf 'Installed: %s\n' "${INSTALL_PREFIX}/bin/kylin-monitor"
