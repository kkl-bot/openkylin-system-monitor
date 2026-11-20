#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Full installed Peony smoke test, only under scripts/test-desktop.sh's isolated HOME/display.

The pointer locations are for a fixed 1280x900 test display and the openKylin default
Chinese locale. Screenshots and on-disk config assertions make every action reviewable.
"""
from pathlib import Path
import json
import os
import subprocess
import sys
import time
sys.excepthook = sys.__excepthook__

PROJECT = Path(sys.argv[1]).resolve()
HOME = Path.home()
assert str(HOME).startswith("/tmp/kylin-monitor-desktop."), "Use scripts/test-desktop.sh, never a user's desktop"
OUTPUT = PROJECT / "docs/previews/native"
OUTPUT.mkdir(parents=True, exist_ok=True)
CAPTURE = PROJECT / "build-openkylin/tests/peony/desktop_capture"
CONFIG = HOME / ".config/org.ukui/peony-qt-desktop/desktop-view.json"
LOG = HOME / "peony-test.log"
processes = []

def run(*args):
    return subprocess.run(list(map(str, args)), check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout

def pointer(*args):
    return run("xdotool", *args)

def wait_for(predicate, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.1)
    raise AssertionError("Timed out waiting for desktop state")

def widgets():
    if not CONFIG.exists():
        return []
    try:
        document = json.loads(CONFIG.read_text())
        return [widget for view in document.get("_views", []) for widget in view.get("widgets", [])
                if widget.get("id") == "org.ukui.kylin-monitor"]
    except (json.JSONDecodeError, OSError):
        return []

def screenshot(name):
    run(CAPTURE, OUTPUT / f"{name}.png")

def start_desktop():
    process = subprocess.Popen(["peony-qt-desktop", "--desktop-window"], stdout=log, stderr=log)
    processes.append(process)
    time.sleep(3)
    assert process.poll() is None, "Peony exited during startup"
    return process

def open_manager():
    # Desktop menu skips the disabled Paste item when navigating via keyboard.
    pointer("mousemove", 650, 100, "click", 3, "sleep", 0.3, "key", "Home", *("Down" for _ in range(9)), "Return")
    def manager():
        result = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", "桌面插件管理|Desktop Widget Manager"], capture_output=True, text=True)
        return result.returncode == 0
    wait_for(manager)
    time.sleep(0.8)
    pointer("mousemove", 243, 508, "click", 1, "sleep", 0.4)

def popup_at(x, y):
    pointer("mousemove", x, y, "click", 3, "sleep", 0.25)

with LOG.open("w") as log:
    try:
        user_dirs = Path(os.environ["XDG_CONFIG_HOME"]) / "user-dirs.dirs"
        user_dirs.write_text(f'XDG_DESKTOP_DIR="{HOME}/Desktop"\n')
        run("gsettings", "set", "org.ukui.style", "style-name", "ukui-light")
        processes.append(subprocess.Popen(["openbox", "--sm-disable"], stdout=log, stderr=log))
        processes.append(subprocess.Popen(["xcompmgr", "-c"], stdout=log, stderr=log))
        time.sleep(0.4)
        desktop = start_desktop()
        open_manager()
        screenshot("manager")
        for x in (500, 710, 960):
            pointer("mousemove", x, 460, "click", 1, "sleep", 0.4)
        pointer("mousemove", 1070, 863, "click", 1, "sleep", 0.5)
        wait_for(lambda: len(widgets()) == 3)
        assert sorted((w["width"], w["height"]) for w in widgets()) == [(136, 136), (296, 136), (296, 296)]
        screenshot("desktop-added")
        # The native surface observes double-clicks while leaving dragging to Peony.
        pointer("mousemove", 1180, 80, "click", "--repeat", 2, "--delay", 100, 1)
        version = run("/usr/bin/kylin-monitor", "--version").strip().split()[-1]
        title = f"Kylin Monitor 任务管理器 · {version}"
        def detail_window():
            found = subprocess.run(["xdotool", "search", "--onlyvisible", "--class", "^kylin-monitor$"],
                                   capture_output=True, text=True)
            return [window for window in found.stdout.splitlines() if pointer("getwindowname", window).strip() == title]
        wait_for(detail_window)
        detail_id = detail_window()[0]
        detail_pid = int(pointer("getwindowpid", detail_id).strip())
        assert Path(f"/proc/{detail_pid}/exe").resolve() == Path("/usr/bin/kylin-monitor"), "Native detail launched a stale user executable"
        os.kill(detail_pid, 15)
        time.sleep(0.3)
        popup_at(1180, 80)
        screenshot("widget-menu")
        pointer("mousemove", 1140, 182, "click", 1)
        popup_at(1180, 80)
        pointer("mousemove", 1140, 466, "click", 1)
        popup_at(1180, 80)
        pointer("mousemove", 1140, 305, "click", 1)
        wait_for(lambda: widgets()[0].get("variantId") == "medium" and widgets()[0].get("interval") == 2000 and widgets()[0].get("paused") is True)
        assert widgets()[0].get("SizeHint") is not None, "Do not overwrite host grid configuration with a JS array"
        popup_at(1100, 480)
        pointer("key", "End", "Return")
        wait_for(lambda: len(widgets()) == 2)
        pointer("mousemove", 1100, 240, "mousedown", 1, "sleep", 0.2)
        for x in (1080, 1040, 980, 900, 800, 700):
            pointer("mousemove", "--sync", x, 240, "sleep", 0.08)
        pointer("sleep", 0.2, "mouseup", 1)
        wait_for(lambda: widgets()[1].get("x", 10) > 10)
        run("gsettings", "set", "org.ukui.style", "style-name", "ukui-dark")
        pointer("mousemove", 500, 800)
        time.sleep(0.6)
        screenshot("desktop-dark")
        saved = widgets()
        (PROJECT / "docs/peony-desktop-state.json").write_text(json.dumps(saved, indent=2, ensure_ascii=False) + "\n")
        desktop.terminate()
        desktop.wait(timeout=5)
        time.sleep(0.4)
        desktop = start_desktop()
        wait_for(lambda: len(widgets()) == 2)
        assert widgets() == saved, "Instance data changed after host restart"
        assert "libkylinmonitorplugin.so" in Path(f"/proc/{desktop.pid}/maps").read_text(), "Restart did not load the native module"
        pointer("mousemove", 500, 800)
        screenshot("desktop-reloaded")
        # The first instance remains at the top-right. Remove it through the host menu.
        popup_at(1180, 80)
        pointer("key", "End", "Return")
        wait_for(lambda: len(widgets()) == 1)
        for mode in ("ukui-light", "ukui-dark"):
            run("gsettings", "set", "org.ukui.style", "style-name", mode)
            run("/usr/bin/kylin-monitor", "--capture-dir", OUTPUT / f"detail-{mode}")
        result = {"host": "installed peony-qt-desktop", "display": "isolated Xvfb 1280x900",
                  "manager_and_three_sizes": True, "menu_pause_interval_and_resize": True,
                  "host_drag": True, "host_instance_removal": True, "dark_theme": True,
                  "double_click_opens_installed_details": True, "details_light_and_dark_capture": True,
                  "restart_preserves_config_and_reloads_module": True}
        (PROJECT / "docs/peony-desktop-result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    except Exception:
        screenshot("desktop-failure")
        print("Last persisted instance state:", json.dumps(widgets(), ensure_ascii=False))
        raise
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
        # Host logs can contain environment paths; keep them out of the public source archive.
