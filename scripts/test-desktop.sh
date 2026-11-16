#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
for command in peony-qt-desktop openbox xcompmgr xdotool xvfb-run dbus-run-session python3; do
    command -v "$command" >/dev/null || { printf 'Missing desktop-test tool: %s\n' "$command" >&2; exit 2; }
done
DESKTOP_TEST_ROOT="$(mktemp -d /tmp/kylin-monitor-desktop.XXXXXX)"
mkdir -p "$DESKTOP_TEST_ROOT/home/Desktop" "$DESKTOP_TEST_ROOT/config" "$DESKTOP_TEST_ROOT/cache" "$DESKTOP_TEST_ROOT/runtime"
chmod 700 "$DESKTOP_TEST_ROOT/runtime"
cleanup() {
    case "$DESKTOP_TEST_ROOT" in
        /tmp/kylin-monitor-desktop.*)
            for portal in doc gvfs; do
                if mountpoint -q "$DESKTOP_TEST_ROOT/runtime/$portal"; then
                    fusermount -uz "$DESKTOP_TEST_ROOT/runtime/$portal"
                fi
            done
            rm -rf -- "$DESKTOP_TEST_ROOT"
            ;;
    esac
}
trap cleanup EXIT
env HOME="$DESKTOP_TEST_ROOT/home" XDG_CONFIG_HOME="$DESKTOP_TEST_ROOT/config" \
    XDG_CACHE_HOME="$DESKTOP_TEST_ROOT/cache" XDG_RUNTIME_DIR="$DESKTOP_TEST_ROOT/runtime" \
    XDG_DATA_DIRS=/usr/local/share:/usr/share QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND=software \
    QT_QPA_PLATFORMTHEME=ukui GSETTINGS_BACKEND=dconf \
    dbus-run-session -- xvfb-run -a -s '-screen 0 1280x900x24' \
    python3 "$PROJECT_DIR/tests/peony/desktop_smoke.py" "$PROJECT_DIR"
