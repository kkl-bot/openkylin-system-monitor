#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${HOME}/.local"
BUILD_DIR="${PROJECT_DIR}/build-openkylin"
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
fi
cmake --build "$BUILD_DIR" --target kylin-monitor --parallel 4
cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX" --component Application
LAUNCHER="${INSTALL_PREFIX}/share/applications/kylin-monitor.desktop"
python3 - "${PROJECT_DIR}/packaging/kylin-monitor.desktop" "$LAUNCHER" "${INSTALL_PREFIX}/bin/kylin-monitor" <<'PY'
from pathlib import Path
import sys

source, destination, executable = sys.argv[1:]

def desktop_string(value):
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")

# Exec has its own quoted-argument grammar after desktop string unescaping;
# TryExec is a single executable path and must not contain argument quotes.
argument = executable.replace("\\", "\\\\").replace('"', '\\"').replace("`", "\\`").replace("$", "\\$").replace("%", "%%")
entries = ["Exec=" + desktop_string('"' + argument + '" --detail'), "TryExec=" + desktop_string(executable)]
result = []
in_entry = found_entry = False
for line in Path(source).read_text(encoding="utf-8").splitlines():
    if line.startswith("["):
        in_entry = line == "[Desktop Entry]"
        if in_entry:
            found_entry = True
            result.extend([line, *entries])
            continue
    if not (in_entry and line.startswith(("Exec=", "TryExec="))):
        result.append(line)
if not found_entry:
    raise SystemExit("Desktop entry template is missing its main group")
Path(destination).write_text("\n".join(result) + "\n", encoding="utf-8")
PY
if command -v xdg-user-dir >/dev/null 2>&1; then
    DESKTOP_DIR="$(xdg-user-dir DESKTOP)"
    if [[ -d "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$HOME" ]]; then
        cp "$LAUNCHER" "${DESKTOP_DIR}/kylin-monitor.desktop"
        chmod +x "${DESKTOP_DIR}/kylin-monitor.desktop"
    fi
fi
printf 'Installed: %s\n' "${INSTALL_PREFIX}/bin/kylin-monitor"
