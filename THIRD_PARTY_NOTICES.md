# Third-party dependencies

Kylin Monitor is licensed under **GPL-3.0-or-later**. The license text is in `LICENSE`.
Original project code, metadata, SVG artwork, scripts, tests and documentation are provided under that license, unless a file states otherwise.

The project uses dynamically loaded or linked system libraries. Their source is not vendored in this repository:

| Dependency | Role | Upstream license / source |
| --- | --- | --- |
| Qt 5 Core, Gui, Widgets, Qml, Quick | C++ and QML application framework | LGPL-3.0 / GPL-3.0 / commercial, depending on module and distribution; https://www.qt.io/licensing/ |
| ukui-quick | Peony widget loading, shared actions, platform theme components | GPL-3.0-or-later; https://gitee.com/openkylin/ukui-quick |
| Peony | Desktop host, widget manager and geometry / instance lifecycle | GPL-3.0-or-later; https://gitee.com/openkylin/peony |
| Linux kernel interfaces | Read-only procfs / sysfs data sources | Runtime operating-system interfaces; no kernel code is included |
| NVIDIA SMI (optional) | External GPU query executable | Separately supplied by NVIDIA; not distributed in this project |

The Peony widget adaptation documentation and installed official widgets were consulted to implement the public host interfaces. No upstream source tree, fonts or official icon assets are installed with the application. The bar icon in `resources/kylin-monitor.svg` is original project artwork. System symbolic icons are resolved by name from the user's theme. Desktop verification screenshots show the system's existing wallpaper and theme; those elements remain the property of their respective authors and are not application resources.

Optional integration tests compile against the installed ukui-quick SDK (or public headers from a separately obtained source tree) and link the installed runtime. They do not bundle the framework. Binary releases must be accompanied by the corresponding complete source, including build and installation scripts, as required by GPLv3. This repository and the generated source archive contain that source; keep it with each matching binary release.
