# Changelog

## 1.4.2 — 2026-11-20

- Give the standalone desktop launcher the distinct Chinese name “Kylin Monitor 任务管理器” and identify the application/version in its window title.
- Open the installed application by default on Windows. Pass `-Build` to rebuild and open the workspace copy, including from paths containing Chinese characters.
- Refresh the build before launching from source, installing for the current user or packaging an existing workspace.
- Support `--version` without a graphical display so installed and workspace versions can be checked independently.
- Verified that the installed application menu entry and desktop shortcut both open `/usr/bin/kylin-monitor` with the 1.4.2 title. All 17 CTest suites passed on openKylin 3.0.

## 1.4.1 — 2026-11-20

- Refine the standalone task manager's light appearance with cool white surfaces, blue-gray structure, ink-blue text and indigo primary actions; retain dark-mode support.
- Use restrained blue process-load shading, align table headings with their values, and improve column spacing while preserving user widths, ordering and safe process controls.
- Add a system search icon when available and distinguish enabled, disabled, focused and selected controls.
- Prefer installed Ubuntu for Latin text and numbers, and STHeiti for Chinese text; retain Noto Sans CJK SC as a Chinese fallback and the existing system-size/window-size typography hierarchy.
- Verified all 16 CTest suites on openKylin 3.0, including live UKUI theme switching and font fallback. Updated the light and dark runtime screenshots in `docs/previews/task-manager-1.4/`.

## 1.4.0 — 2026-11-20

- Separate global navigation, performance resource selection and resource details in a Windows 11 task-manager-inspired layout.
- Place process names, CPU usage and resident memory first in new table layouts. Color CPU and memory cells by the process share and keep existing column preferences on upgrade.
- Remember the selected performance resource and the navigation panel's expanded or collapsed state.
- Put the primary utilization chart directly below the resource/device heading; keep performance controls in a persistent workspace footer.
- Show memory composition using sampled used/available capacity and explicit percentages, without inventing unsupported memory categories or historical capacity data.
- Preserve CPU/GPU metrics and temperature information, and use a compact resource picker when the window cannot fit the resource list.
- Capture real CPU, memory and process layouts at three window sizes and with enlarged fonts; collect current network screenshots in `docs/previews/task-manager-1.4/` for release packaging.

## 1.3.1 — 2026-11-20

- Apply one typography hierarchy to page titles, metrics, body text, captions, charts and read-only dialogs.
- Derive the window's body font from the system font and available window size; avoid repeated or independent font multipliers.
- Recalculate process tables, network rate cards and chart layouts after font changes while keeping controls reachable in compact windows.
- Capture current light/dark layouts in `docs/previews/typography/` and include the large-font CPU view in the submission archive, preserving historical screenshots.

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
