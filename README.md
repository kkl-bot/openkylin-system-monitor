# Kylin Monitor · Peony 系统监测插件

C++17 编写的 CPU、GPU、内存监测工具，包含 **Peony 原生桌面插件**与任务管理器风格的详细窗口。使用 **GPL-3.0-or-later** 许可证，硬件采集、数据模型、视图和桌面集成分层，声明与实现分文件。

![原生浅色模式](docs/previews/native/ukui-light-medium.png)
![原生深色模式](docs/previews/native/ukui-dark-medium.png)

## 环境与功能

目标环境：**openKylin 3.0 / Peony 4.24 / ukui-quick 4.25 / Qt 5.15 / amd64**。插件采用当前 Peony 的 `ManagerSizes`、标准 `WidgetItem` 与 `Widget.actions`，由系统负责预览、拖动、碰撞检测、菜单和实例生命周期。

| 模式 | 尺寸 | 操作 |
| --- | --- | --- |
| 原生小尺寸 | 136 × 136 | 管理器添加 / 右键切换 |
| 原生中尺寸 | 296 × 136 | 同上，默认尺寸 |
| 原生大尺寸 | 296 × 296 | 同上 |
| 详细窗口 | 可调整大小 | 双击插件 / Enter / 右键查看详情 |

三档均只显示柱状图：左到右是蓝色 CPU、绿色 GPU、紫色内存；每组左柱利用率（0–100%），右柱温度（0–120 °C）。温度高于上限时柱条封顶，悬停仍显示原始数值。0 是空柱，不可用是斜纹柱。

使用 UKUI `StyleBackground`、`Theme` 系统配色和字体，菜单由宿主统一显示，支持运行时深浅主题切换。详细窗口也跟随系统主题。右键提供更多插件、详情、暂停、GPU 切换、尺寸、刷新频率和移除；空格暂停 / 继续。尺寸切换前检查空间与碰撞，空间不足时禁用目标尺寸。

## 编译、安装与添加

在 openKylin 内执行：

```bash
git clone https://gitee.com/kkl-bot/openKylin-peeker.git
cd openKylin-peeker
sudo apt-get install build-essential cmake python3 qtbase5-dev qtdeclarative5-dev libqt5svg5-dev
bash scripts/build.sh
bash scripts/package.sh
sudo apt install ./dist/kylin-monitor-1.1.0-linux-amd64.deb
```

或者执行 `bash scripts/install.sh`，一次完成编译、测试、打包和安装。运行依赖由安装包声明，包含 Peony、ukui-quick 与 Qt Quick 模块。

首次安装或升级后，**注销并重新登录 openKylin**，让 Peony 重新发现插件并加载新动态库。桌面空白处右键 → **更多插件** → **系统监测** → 选择尺寸添加，也可以从管理器拖放到桌面。三档预览与真实实例共用同一个 QML 入口。

原生插件必须在 Peony 桌面会话中添加。已有远程桌面环境可连接 `localhost:3390` 后操作。通过 WSLg 直接打开独立窗口，不等于添加原生插件。

## 持久化与用户数据

- 位置、尺寸与实例 ID 由 Peony 保存。
- 刷新间隔、暂停状态、GPU 选择及配置版本写入当前实例的 `Widget.config`；各实例独立，重新登录或加载后恢复。
- 桌面配置通常位于 `~/.config/org.ukui/peony-qt-desktop/`。不要为清理一个插件而删除整个桌面配置目录。
- 管理器预览不读写真实实例配置，也不能执行移除、尺寸切换或打开详情等动作。
- 详细窗口的模式、位置和刷新间隔通过独立 `QSettings` 保存。
- 不读取用户文件，不上传数据。采样值与最近 60 秒历史仅在内存中；本项目没有便签等用户编辑内容。
- 卸载保留用户配置；主动移除桌面实例时，由 Peony 清除该实例配置，新建实例使用默认值。

## 卸载、重载与升级

先右键插件 → **移除本插件**，然后执行：

```bash
sudo apt remove kylin-monitor
# 或 bash scripts/uninstall.sh
```

包管理器移除程序、元数据、QML 模块、图标和许可文档。已加载的库可能在 Peony 进程内保留至注销，因此卸载 / 升级后重新登录；脚本不会强制结束用户桌面。重新安装并登录后可再次添加。

旧版 1.0 用户级独立应用不受系统包管理器管理。`scripts/install-user.sh` 仅安装独立应用，不安装原生插件；原生部署请使用系统包。

## 数据源与限制

| 指标 | 数据源 |
| --- | --- |
| CPU 利用率 | `/proc/stat` 两次差值，不重复累加 guest |
| CPU 温度 | 已识别的 CPU hwmon / 明确 CPU thermal zone |
| 内存利用率 | `(MemTotal − MemAvailable) / MemTotal`，旧内核使用注明的估算 |
| 内存温度 | `jc42` / `spd5118` / `spd5hub` 传感器 |
| NVIDIA GPU | 可选 `nvidia-smi`，支持多卡、N/A 和超时 |
| AMD / 其他 DRM GPU | `gpu_busy_percent`、VRAM 和设备 hwmon |

缺失读数明确显示为不可用，不用电池或硬盘温度代替 CPU / 内存温度。Intel GPU 尚未实现专用利用率采集器，仅在驱动提供兼容接口时可读。内存温度需要硬件传感器和驱动。

当前 WSL 实测能读 CPU、内存利用率，未暴露 GPU 监测及 CPU / 内存温度；这些数据反映 WSL 环境，而非 Windows 宿主完整负载。裸机 GPU 的解析路径有夹具测试，仍需对应硬件实机验收。

## 测试

```bash
cmake -S . -B build-openkylin -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build-openkylin --parallel 4
ctest --test-dir build-openkylin --output-on-failure
```

插件固定使用 Qt 5，不能将 Qt 6 动态库加载进 Qt 5 Peony。只构建独立应用可用 `-DBUILD_PEONY_PLUGIN=OFF`，支持 Qt 5 / Qt 6。

可选的真实框架集成测试：

```bash
sudo apt-get install libukui-quick-dev xvfb xauth dbus-x11 qml-module-qttest
bash scripts/build.sh -DBUILD_PEONY_INTEGRATION_TESTS=ON
```

SDK 与运行库需匹配。已有对应源码头文件时，可额外指定 `-DUKUI_QUICK_SOURCE_DIR=/path/to/ukui-quick`，该路径仅供测试，不打包第三方源码。测试脚本隔离配置、D-Bus 和显示，不改变用户主题。Ubuntu CI 只证明构建及非宿主测试通过；原生框架和完整桌面验收见 [验证记录](docs/verification.md)。

安装本项目 Debian 包、启用上述集成测试构建后，可运行完整桌面验收：

```bash
sudo apt-get install --no-install-recommends openbox xcompmgr xdotool xvfb xauth dbus-x11
bash scripts/test-desktop.sh
```

脚本启动独立的 Peony 进程和临时用户目录，检查真实管理器添加、菜单、拖动、双击详情、移除和宿主重启恢复，结果及截图写入 `docs/`。它使用默认中文界面、固定 1280×900 屏幕；其他语言或布局需要调整测试坐标。Openbox 和 xcompmgr 仅为无桌面测试显示提供窗口管理和合成，不属于插件运行依赖。脚本中的宿主重启仅针对测试进程。

## 源码与发布

```text
src/domain/       公共数据模型
src/backend/      采集器、温度和文件接口
src/ui/           详细窗口、历史曲线与备用悬浮窗
src/app/          独立应用接线、托盘与设置
plugin/           metadata.json、main.qml、qmldir
plugin/src/       C++ QML 数据桥接、共享采样与柱条绘制
tests/            采集、界面、插件与原生框架测试
resources/        原创 SVG 图标
scripts/          构建、安装、卸载、验证与源码归档
packaging/        独立应用 desktop 入口
docs/             架构、验收与真实截图
```

多个桌面实例和预览共享采样服务，按最短请求间隔采集；无消费者时暂停。QML 负责系统接口与布局，数据解析和柱状图绘制均在 C++ 中。

```bash
python3 scripts/prepare-release.py
```

生成 `dist/kylin-monitor-1.1.0-source.tar.gz` 与 `SHA256SUMS-1.1.0.txt`。同目录的本版本 Debian 包也纳入校验。公开文件白名单排除了构建目录、临时文件、Git 元数据及含环境凭据的原始需求文件，`.gitignore` 也排除了这些内容。发布二进制时需一并提供对应完整源码和构建脚本。

项目仓库：[Gitee · openKylin-peeker](https://gitee.com/kkl-bot/openKylin-peeker)。源码使用 Git 管理，构建产物和安装包放在被忽略的 `dist/` 中，发布时作为发行附件提供。请勿把本机需求、凭据、用户配置或构建目录提交到仓库。

`.editorconfig` 和 `.gitattributes` 统一 UTF-8、缩进与换行：Linux 源码 / 脚本使用 LF，Windows 启动脚本使用 CRLF。附带的 GitHub Actions 工作流供 GitHub 镜像使用；上传到 Gitee 不会自动执行该工作流。见 [贡献说明](CONTRIBUTING.md)、[变更记录](CHANGELOG.md) 和 [架构说明](docs/architecture.md)。

## 独立应用与诊断

```bash
/usr/bin/kylin-monitor --detail
/usr/bin/kylin-monitor --mini
/usr/bin/kylin-monitor --probe
```

`--mini` 保留原先的独立置顶悬浮窗。`--probe` 输出两次真实采样后的 JSON，缺失为 `null` 并附原因。

## 许可与参考

[GPL-3.0-or-later](LICENSE)；依赖说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

- [Peony 当前插件适配文档](https://gitee.com/openkylin/peony/blob/openkylin/huanghe/peony-qt-desktop/desktop-widgets/base/widget-manager-plugin-adaptation.md)
- [openKylin 插件开发指南](https://www.openkylin.top/news/3593-cn.html)
- [ukui-quick 官方源码](https://gitee.com/openkylin/ukui-quick)
- [Linux hwmon 接口](https://docs.kernel.org/hwmon/sysfs-interface.html)
- [NVIDIA WSL 指南](https://docs.nvidia.com/cuda/wsl-user-guide/)
