# 验证记录

验证日期：2026-11-16（Asia/Shanghai），版本 1.1.0。

## 环境与依据

- openKylin 3.0（huanghe），WSL 2，amd64。
- Peony `4.24.0.0-ok8`，ukui-quick framework `4.25.0.0-ok0.2`。
- Qt 5.15.10，GCC 15.2.0，CMake 4.2.3。
- 依据 [Peony 当前插件适配说明](https://gitee.com/openkylin/peony/blob/openkylin/huanghe/peony-qt-desktop/desktop-widgets/base/widget-manager-plugin-adaptation.md)，阅读时提交 `48e4ad372a0cb5250f715feb1a45fd51f7cd1115`。
- 完整桌面验收使用已安装的 Peony 可执行文件、实际系统运行库及安装后的插件；显示为独立 Xvfb 1280×900，临时 HOME、D-Bus、配置与主题，不操作用户的 XRDP 桌面。

## 规范逐项验收

| 需求 | 实现及验证 |
| --- | --- |
| 原生插件发现 | 元数据、QML 模块、系统图标安装到标准路径，实际管理器可见“系统监测” |
| 系统标准尺寸 | 管理器添加 136×136、296×136、296×296 三档；预览与桌面共用入口 |
| 尺寸切换 | 系统共享菜单切换，先调用宿主边界 / 碰撞检查；框架测试验证拒绝冲突切换 |
| 原生交互 | 共享 `Widget.actions`，完整桌面验证菜单、拖动、双击详情和移除 |
| 浅色 / 深色 | 实际 UKUI Theme 运行时切换，柱条、背景和详细窗口截图验证 |
| 配置持久化 | 保存尺寸、位置、刷新间隔、暂停及 GPU 选择；销毁重建测试和完整宿主重启均验证恢复 |
| 预览隔离 | 三档预览不写实例配置，所有修改操作禁用 |
| 安装与卸载 | Debian 包检查依赖、权限和文件清单，实际卸载后检查标准安装路径，再重新安装 |
| 重新加载 | 重启测试宿主，读取 `/proc/<pid>/maps` 确认重新载入安装后的动态库，并比较实际桌面配置 |
| 仓库材料 | 完整 C++ / QML 源码、元数据、原创 SVG、README、架构说明、GPL-3.0-or-later、第三方说明、测试与 CI、发布归档脚本 |

以上是目标环境的项目验收结果，不代表获得发行版官方认证。

## 自动测试

Release 构建后运行 CTest，四组测试包含 24 个功能用例（不计 QtTest 初始化 / 清理）：

- 采集器 11 项：CPU 差值及 guest、计数器回退、内存估算、温度分类、坏传感器、NVIDIA 多卡 / 缺失值、AMD、无 GPU、超时和采样生命周期。
- 应用视图 3 项：历史过期 / 设备移除、详细窗口交互、独立极简窗口交互。
- 原生数据桥 5 项：缺失与 0 的区别、GPU 断连选择、刷新 / 暂停、主题绘制、双击不吞宿主事件。
- 实际 ukui-quick 5 项：发现、全部尺寸与预览隔离、共享动作与碰撞拒绝、配置写盘 / 重建、动态主题。

完整桌面脚本 `scripts/test-desktop.sh` 从真实管理器添加三种尺寸，使用宿主菜单修改实例、拖动、移除、切换主题，重启 Peony 后验证配置与模块加载；还验证双击启动 `/usr/bin/kylin-monitor`，生成详细窗口两种主题的真实采样截图。结构化结果保存在 [桌面验收结果](peony-desktop-result.json)，重启前实例配置快照见 [配置快照](peony-desktop-state.json)。测试脚本依赖及重现命令见 README。

测试中发现并修复了三个集成问题：JS 数组覆写宿主 `SizeHint` 导致配置空值、指针处理器拦截拖动、用户 PATH 中旧程序遮蔽系统详情程序。退出阶段还观察到 UKUI 样式库在 QQmlThread 中延迟处理主题消息；框架测试在 QApplication 销毁前留出消息处理时间，避免测试立刻退出引发系统样式库竞态。

## 截图

- [真实插件管理器](previews/native/manager.png)
- [添加三种尺寸](previews/native/desktop-added.png)
- [宿主共享右键菜单](previews/native/widget-menu.png)
- [深色桌面](previews/native/desktop-dark.png)
- [宿主重启后](previews/native/desktop-reloaded.png)
- [浅色详细窗口](previews/native/detail-ukui-light/cpu.png)
- [深色详细窗口](previews/native/detail-ukui-dark/cpu.png)

原生卡片三档的独立截图亦位于 `docs/previews/native/`。截图使用真实采样，不填充虚构温度或 GPU 数据；短时间截图的历史曲线仅出现在 60 秒轴最右端。

## 硬件与验收边界

当前 WSL 的 `/sys/class/drm` 只有 `version`，hwmon 设备为 `AC1`、`BAT1`，没有可调用的 `nvidia-smi`。CPU / 内存利用率实测有效，GPU 与 CPU / 内存温度缺失，界面显示不可用斜纹并说明原因。数据反映 WSL 而非 Windows 宿主完整负载。

NVIDIA / AMD 路径已用夹具验证，仍需裸机传感器和对应 GPU 的实机验收。Intel GPU 没有专用利用率采集器；内存温度需要传感器和驱动。未在用户 `localhost:3390` 的 XRDP 会话、多屏、其他缩放、Wayland、其他 Peony 版本下做交互验收。

测试仅启动和重启自己创建的 Peony 进程，未重启用户桌面。安装 / 升级后的正常使用请注销并重新登录，在桌面“更多插件”中添加。
