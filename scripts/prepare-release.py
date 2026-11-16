#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a source-only archive from an explicit public-file allowlist."""
from pathlib import Path
import hashlib
import json
import re
import tarfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = re.search(r"project\(KylinMonitor VERSION ([^ ]+)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
METADATA = json.loads((ROOT / "plugin/metadata.json").read_text(encoding="utf-8"))
assert METADATA["Version"] == VERSION, "Metadata version does not match CMake"
assert METADATA["Id"] == "org.ukui.kylin-monitor"
assert METADATA["ShowIn"] == "Desktop"
assert METADATA["Contents"]["ShowInManager"] is True
assert [entry["id"] for entry in METADATA["Contents"]["ManagerSizes"]] == ["small", "medium", "large"]
assert (ROOT / "plugin" / METADATA["Contents"]["Main"]).is_file()
assert "GNU GENERAL PUBLIC LICENSE" in (ROOT / "LICENSE").read_text(encoding="utf-8")

PUBLIC_PATHS = ["CMakeLists.txt", "LICENSE", "README.md", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "CONTRIBUTING.md",
                ".gitignore", ".gitattributes", ".editorconfig", ".github", "src", "plugin", "resources", "packaging", "scripts", "tests", "docs", "启动小组件.ps1"]
files = []
for name in PUBLIC_PATHS:
    path = ROOT / name
    if not path.exists():
        raise FileNotFoundError(f"Missing release file: {name}")
    files.extend([path] if path.is_file() else (entry for entry in path.rglob("*") if entry.is_file()))
files = sorted(file for file in files if "__pycache__" not in file.parts and file.suffix not in {".pyc", ".log"})
DIST = ROOT / "dist"
DIST.mkdir(exist_ok=True)
archive = DIST / f"kylin-monitor-{VERSION}-source.tar.gz"
with tarfile.open(archive, "w:gz") as target:
    for file in files:
        entry = target.gettarinfo(str(file), arcname=f"kylin-monitor-{VERSION}/{file.relative_to(ROOT).as_posix()}")
        entry.uid = entry.gid = 0
        entry.uname = entry.gname = "root"
        entry.mode = 0o755 if file.suffix in {".sh", ".py"} else 0o644
        with file.open("rb") as stream:
            target.addfile(entry, stream)
with tarfile.open(archive) as source:
    names = source.getnames()
    assert not any("需求.md" in name or "/.git/" in name or "/build-" in name for name in names)
    assert any(name.endswith("/plugin/metadata.json") for name in names)
    assert any(name.endswith("/LICENSE") for name in names)
checksum_lines = []
for path in sorted(DIST.glob(f"kylin-monitor-{VERSION}-*")):
    if path.is_file():
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        checksum_lines.append(f"{digest}  {path.name}")
(DIST / f"SHA256SUMS-{VERSION}.txt").write_text("\n".join(checksum_lines) + "\n", encoding="utf-8")
print(f"Validated metadata; archived {len(files)} public source files: {archive}")
