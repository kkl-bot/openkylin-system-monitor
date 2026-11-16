# Changelog

## 1.1.0 — 2026-11-16

- Add a native Peony / ukui-quick desktop widget with system-managed small, medium and large variants.
- Use shared system context menus, platform theme colors, dark mode and host-managed dragging / instance removal.
- Persist per-instance refresh, pause, GPU and geometry settings through Peony configuration.
- Reuse C++ collectors in the QML backend, with one shared sampling service across widget instances.
- Add a GPL-3.0-or-later license, dependency notices, release-source archive tooling, native integration tests and build CI.
- Keep the detailed performance window and standalone mini mode; make application surfaces follow system light / dark colors.

## 1.0.0 — 2026-11-15

- Initial C++17 / Qt Widgets monitor with CPU, GPU and memory collectors and two standalone window modes.
