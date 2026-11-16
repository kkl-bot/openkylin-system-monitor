#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
if [[ "$(id -u)" == 0 ]]; then
    apt-get remove -y kylin-monitor
else
    sudo apt-get remove -y kylin-monitor
fi
printf '%s\n' 'Package removed. Per-user configuration is preserved for reinstall.'
printf '%s\n' 'Remove the widget instance in Peony before uninstalling; log out/in to unload its shared library.'
