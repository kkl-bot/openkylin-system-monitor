#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bundle the matching source, Debian package, instructions and real screenshots."""
from pathlib import Path
import hashlib
import json
import re
import runpy
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = re.search(r"project\(KylinMonitor VERSION ([^ ]+)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
DIST = ROOT / "dist"
DEB = DIST / f"kylin-monitor-{VERSION}-linux-amd64.deb"
if not DEB.is_file():
    raise FileNotFoundError("Build the matching Debian package with scripts/package.sh first")
runpy.run_path(str(ROOT / "scripts/prepare-release.py"), run_name="__main__")

prefix = f"kylin-monitor-{VERSION}-MVP/"
files = {}
modes = {}
with tarfile.open(DIST / f"kylin-monitor-{VERSION}-source.tar.gz") as archive:
    for member in archive.getmembers():
        if not member.isfile():
            continue
        relative = member.name.split("/", 1)[1]
        name = "source/" + relative
        files[name] = archive.extractfile(member).read()
        modes[name] = member.mode
files[f"packages/{DEB.name}"] = DEB.read_bytes()
for theme in ("light", "dark"):
    screenshot = ROOT / f"docs/previews/task-manager/{theme}/processes.png"
    adaptive = ROOT / f"docs/previews/window-layout/{theme}/processes-1180x780.png"
    if adaptive.exists():
        screenshot = adaptive
    files[f"screenshots/task-manager-{theme}.png"] = screenshot.read_bytes()
    for name in ("network-1180x780", "network-760x500", "network-details"):
        screenshot = ROOT / f"docs/previews/network/{theme}/{name}.png"
        files[f"screenshots/{name}-{theme}.png"] = screenshot.read_bytes()

files["提交说明.md"] = f"""# Kylin Monitor {VERSION} · 任务管理器 MVP

源码与完整 README 位于 source/；Debian 安装包位于 packages/；真实进程和网络界面截图位于 screenshots/。
source/docs/previews/ 保留完整的性能窗口和 Peony 插件历史验收截图。

在 openKylin 3.0 amd64 内执行：

```bash
sudo apt install ./packages/{DEB.name}
/usr/bin/kylin-monitor --processes
/usr/bin/kylin-monitor --network
```

进程页支持搜索、按 CPU / 内存 / 磁盘读写排序、详情查看及确认后结束当前普通用户进程。
网络页显示按网卡采集的上传、下载速度和代理配置状态；“详细信息”包含 IP、网关、DNS 和子网掩码。
左侧 CPU、GPU、内存打开性能曲线；Ctrl+M 切换备用柱状图悬浮窗。
安装或升级后注销并重新登录，可在桌面“更多插件”中添加原生 Peony 小插件。
卸载命令：sudo apt remove kylin-monitor。用户配置会保留。

使用及验证说明：source/docs/network-monitor.md、source/docs/network-verification.md、source/docs/task-manager.md。
本版本在 openKylin WSL 中验证；监测范围为 Linux 环境，不能管理 Windows 宿主进程。
WSL 未提供的温度或 GPU 指标显示不可用。开源许可证为 GPL-3.0-or-later，详见 source/LICENSE。

MANIFEST.json 提供每个归档文件的 SHA-256；外部 SHA256SUMS-{VERSION}.txt 用于校验 ZIP 和其他发布文件。
""".encode("utf-8")
manifest = {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} for name, data in sorted(files.items())}
files["MANIFEST.json"] = (json.dumps({"version": VERSION, "files": manifest}, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
target = DIST / f"kylin-monitor-{VERSION}-MVP-submission.zip"
with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, data in sorted(files.items()):
        entry = zipfile.ZipInfo(prefix + name)
        entry.create_system = 3
        entry.external_attr = (0o100000 | modes.get(name, 0o644)) << 16
        entry.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(entry, data)
with zipfile.ZipFile(target) as archive:
    assert archive.testzip() is None
    for name, expected in manifest.items():
        data = archive.read(prefix + name)
        assert hashlib.sha256(data).hexdigest() == expected["sha256"]
checksums = []
for path in sorted(DIST.glob(f"kylin-monitor-{VERSION}-*")):
    if path.is_file():
        checksums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}")
(DIST / f"SHA256SUMS-{VERSION}.txt").write_text("\n".join(checksums) + "\n", encoding="utf-8")
print(f"Verified ZIP: {target} ({len(files)} files, {target.stat().st_size} bytes)")
