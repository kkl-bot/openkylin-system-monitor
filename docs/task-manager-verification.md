# 任务管理器 1.2.0 验证记录

验证日期：2026-11-16（Asia/Shanghai）。环境为 openKylin 3.0 WSL 2 amd64，
Qt 5.15.10、GCC 15.2.0、Peony 4.24.0.0-ok8、ukui-quick 4.25.0.0-ok0.2。

## 构建与自动测试

Release 编译成功，CTest **8 / 8 组通过**，共 **49 个功能用例**，不计 QtTest
初始化和清理；无失败、无跳过。本轮 CTest 总耗时 8.96 秒。

| 测试组 | 功能用例 | 验证内容 |
| --- | ---: | --- |
| collectors | 11 | 原有 CPU、GPU、内存、传感器及资源采样生命周期 |
| ui | 3 | 性能窗口、历史曲线、独立悬浮窗交互 |
| process_backend | 10 | procfs 解析、计数重置、缺失值、PID 复用、身份和权限、真实信号 |
| process_service | 6 | 后台采样、隐藏、暂停、手动刷新、基线恢复、过期快照及退出 |
| process_ui | 8 | 数值排序、筛选、选择保持、操作保护、确认目标、中文状态及表头恢复 |
| task_manager_workflow | 1 | 实际采集 → 按 PID 搜索 → 选择 → 默认取消的确认框 → 结束专用子进程；页面切换、最小化及恢复 |
| widget_backend | 5 | 原生数据桥、不可用指标、GPU 选择、主题和输入事件 |
| peony_integration | 5 | 实际 ukui-quick 加载、标准尺寸、动作、持久化及动态主题 |

真实结束测试只操作测试自身创建的 `/bin/sleep` 子进程。SIGTERM 与 SIGKILL 均已验证；
没有结束其他应用或重启用户桌面。进程操作测试以普通 openkylin 用户执行。

复现命令：

```bash
bash scripts/build.sh -DBUILD_PEONY_INTEGRATION_TESTS=ON \
    -DUKUI_QUICK_SOURCE_DIR=/path/to/matching/ukui-quick
```

匹配的官方开发包已安装时可以省略 `UKUI_QUICK_SOURCE_DIR`。普通构建和测试不需要这组
可选宿主 SDK；详见 README。工作流测试以 root 运行时会跳过真实终止场景，因为产品
故意将 root 进程设为只读。

## 真实运行截图

通过 `bash scripts/capture-task-manager.sh` 运行实际构建程序，读取当前 Linux `/proc`，
等待 3.5 秒后保存窗口。使用独立 Xvfb、D-Bus、用户配置和 UKUI 主题，不改动 XRDP
会话。数据未使用固定夹具或随机数，显示的是 WSL Linux 进程。

- [浅色进程页](previews/task-manager/light/processes.png)
- [深色进程页](previews/task-manager/dark/processes.png)
- [浅色 CPU 性能页](previews/task-manager/light/cpu.png)
- [深色 CPU 性能页](previews/task-manager/dark/cpu.png)

这组截图为应用窗口抓图，不包含桌面装饰。1.1 完整 Peony 桌面交互的历史证据仍见
[原生插件验证记录](verification.md)；本轮重新运行了宿主框架测试，没有重复完整桌面
坐标自动化测试。

## 边界

- 这是进程管理与性能监控 MVP，未实现服务管理、开机启动管理或批量进程树操作。
- 进程页仅管理当前 Linux 环境，不读取或终止 Windows 宿主进程。
- 仅允许结束当前普通用户符合条件的进程；不提权，不允许结束 root、PID 1 或自身。
- 当前 WSL 未暴露 GPU 监测及 CPU / 内存温度，对应项保持不可用；具体硬件限制沿用
  [原生插件验证记录](verification.md)。
- 截图环境的 portal / RealtimeKit 提示来自系统服务，未导致程序退出或上述测试失败。

新增功能分别置于 `process_*` 领域模型、采集、控制、后台服务、视图和应用控制器文件，
原文件只增加导航、接线、配置、构建与发布信息。声明与实现分离，详细结构见
[架构说明](architecture.md) 和 [任务管理器说明](task-manager.md)。
