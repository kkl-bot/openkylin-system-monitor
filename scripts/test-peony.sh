#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
TEST_BINARY="${1:?Pass the peony_integration_tests executable}"
shift
TEST_ROOT="$(mktemp -d /tmp/kylin-monitor-test.XXXXXX)"
cleanup() {
    case "$TEST_ROOT" in
        /tmp/kylin-monitor-test.*)
            for portal in doc gvfs; do
                if mountpoint -q "$TEST_ROOT/runtime/$portal"; then
                    # The isolated D-Bus session may unmount its portal between these two calls.
                    if ! fusermount -uz "$TEST_ROOT/runtime/$portal" && mountpoint -q "$TEST_ROOT/runtime/$portal"; then
                        return 1
                    fi
                fi
            done
            rm -rf -- "$TEST_ROOT"
            ;;
    esac
}
trap cleanup EXIT
mkdir -p "$TEST_ROOT/config" "$TEST_ROOT/cache" "$TEST_ROOT/runtime"
chmod 700 "$TEST_ROOT/runtime"
env XDG_CONFIG_HOME="$TEST_ROOT/config" XDG_CACHE_HOME="$TEST_ROOT/cache" \
    XDG_RUNTIME_DIR="$TEST_ROOT/runtime" XDG_DATA_DIRS=/usr/local/share:/usr/share QT_QUICK_BACKEND=software QT_QPA_PLATFORM=xcb \
    QT_QPA_PLATFORMTHEME=ukui GSETTINGS_BACKEND=dconf \
    dbus-run-session -- xvfb-run -a -s '-screen 0 1280x900x24' "$TEST_BINARY" "$@"
