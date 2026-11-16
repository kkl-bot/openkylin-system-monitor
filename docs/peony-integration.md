# Peony 集成契约

本项目采用 openKylin 3.0 当前 Peony 的管理器契约。依据为官方 `peony` 的 `openkylin/huanghe` 分支及本机运行库，阅读时源码提交为 `48e4ad372a0cb5250f715feb1a45fd51f7cd1115`。

上游适配说明：
https://gitee.com/openkylin/peony/blob/openkylin/huanghe/peony-qt-desktop/desktop-widgets/base/widget-manager-plugin-adaptation.md

## 安装结果

```text
/usr/bin/kylin-monitor
/usr/share/ukui/widgets/org.ukui.kylin-monitor/metadata.json
/usr/share/ukui/widgets/org.ukui.kylin-monitor/main.qml
/usr/lib/x86_64-linux-gnu/qt5/qml/org/ukui/kylinmonitor/qmldir
/usr/lib/x86_64-linux-gnu/qt5/qml/org/ukui/kylinmonitor/libkylinmonitorplugin.so
/usr/share/icons/hicolor/scalable/apps/kylin-monitor.svg
/usr/share/applications/kylin-monitor.desktop
/usr/share/doc/kylin-monitor/{LICENSE,README.md,THIRD_PARTY_NOTICES.md}
```

QML 路径默认由目标 Qt 5 的 qmake 查询，可通过 CMake 的 `KYLIN_MONITOR_QML_INSTALL_DIR` 覆盖。原生部署使用 `/usr` 前缀和系统包管理器，独立应用的用户安装脚本不宣称安装了原生插件。

## 当前宿主约定

| 项目 | 实现 |
| --- | --- |
| 插件标识 | `org.ukui.kylin-monitor`，目录与元数据一致 |
| 宿主过滤 | `ShowIn: Desktop` |
| 管理器可见性 | `Contents.ShowInManager: true` |
| 标准尺寸 | `ManagerSizes`：small / medium / large |
| 入口 | `Contents.Main: main.qml`，根项 `WidgetItem` |
| 预览 | 与真实桌面同一个入口，宿主按 12/17 缩放 |
| 菜单 | `Widget.actions`，宿主创建与定位 |
| 位置和拖动 | `Dragable: true`，由 Peony 委托处理 |
| 缩放 | `Resizable: false`，只允许三档固定尺寸 |
| 切换 | 先检查 `canResizeWidgetTo`，再写入变体与宽高 |
| 删除 | 调用 `removeWidgetInstance(instanceId)` |
| 主题 | UKUI `Theme` / `StyleBackground`，运行时响应 |
| 配置 | `Widget.config`，预览实例禁止写入 |

在线的旧开发模板使用 `ManagerVariants`、独立 `previewMain` 和自建延迟菜单。当前宿主已改用共用入口、`ManagerSizes` 和共享动作菜单，所以本项目不机械复制这些旧模板。三档桌面尺寸依次为 136×136、296×136、296×296；预览尺寸由系统计算。

## 验收层级

1. 后端与视图单元测试：数据解析、缺失值、投影、暂停和主题绘制。
2. 实际 ukui-quick 加载器测试：发现、三个尺寸与预览隔离、宿主动作、配置写盘与重建、动态主题。
3. 完整 Peony 冒烟测试：从真实管理器添加，操作菜单、拖动、移除，重新启动宿主后检查配置和模块加载。
4. Debian 包生命周期：安装产物、依赖、权限、卸载和重新安装。

完整桌面脚本使用隔离 HOME / D-Bus / Xvfb，不在用户当前会话中添加测试实例。脚本固定 openKylin 默认中文界面和 1280×900 虚拟屏幕，用截图及实际 JSON 断言保留结果。它不替代不同缩放比例、多屏、Wayland 或真实硬件的后续验收。
