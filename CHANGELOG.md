# Changelog

## 1.3.0 — 2026-11-18

- Add a network page with per-interface download/upload rates and traffic history.
- Show desktop proxy mode separately from inherited proxy environment variables.
- Add read-only network details: IPv4/IPv6 addresses, masks/prefixes, gateways and DNS sources.
- Keep network collection, proxy detection, background service and views in separate modules.

## 1.2.1 — 2026-11-18

- Reflow controls and performance cards in smaller windows, keeping long text reachable by scrolling.
- Size process columns, rows, resource tiles and chart labels using font metrics; preserve manually adjusted columns.
- Show complete, escaped text in hover hints and wrap long process command lines.
- Add real-data layout captures for compact, standard and enlarged-font windows.

## 1.2.0 — 2026-11-16

- Add a task-manager process page alongside the existing performance charts.
- Show process identity, owner, state, CPU share, resident memory, threads and disk read/write rates, with numeric sorting and filtering.
- Add read-only process details and confirmed graceful / force termination for eligible processes owned by the current user, using pidfd and start-time checks.
- Sample processes in a dedicated worker only while the process page is visible; support pause and manual refresh.
- Keep process collection, control, models, dialogs and controller wiring in separate new files, with dedicated tests.

## 1.1.0 — 2026-11-16

- Add a native Peony / ukui-quick desktop widget with system-managed small, medium and large variants.
- Use shared system context menus, platform theme colors, dark mode and host-managed dragging / instance removal.
- Persist per-instance refresh, pause, GPU and geometry settings through Peony configuration.
- Reuse C++ collectors in the QML backend, with one shared sampling service across widget instances.
- Add a GPL-3.0-or-later license, dependency notices, release-source archive tooling, native integration tests and build CI.
- Keep the detailed performance window and standalone mini mode; make application surfaces follow system light / dark colors.

## 1.0.0 — 2026-11-15

- Initial C++17 / Qt Widgets monitor with CPU, GPU and memory collectors and two standalone window modes.
