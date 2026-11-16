# Contributing

Use C++17 and keep declarations in headers and implementations in source files. Hardware collection belongs in `src/backend`, domain values in `src/domain`, desktop integration in `plugin`, and standalone views in `src/ui`. Reuse the collectors; do not read hardware from QML or draw fabricated values for missing sensors.

Run `bash scripts/build.sh` before submitting a change. Native host changes also require the integration tests described in the README on openKylin. Ubuntu CI validates compilation and non-host tests; it does not prove Peony desktop behavior.

For bug reports, include openKylin / Peony / Qt versions, steps to reproduce, whether the problem affects preview or desktop instances, and the relevant error output. Do not attach credentials or a full user configuration directory.

Before a release, update `CMakeLists.txt`, `plugin/metadata.json`, `src/main.cpp` and `CHANGELOG.md` together. Build and test, produce the Debian package, then run `python3 scripts/prepare-release.py`. Publish the matching source archive, binary package and checksum file together.

Contributions to this repository are provided under GPL-3.0-or-later, as described in `LICENSE`.
