# 架构与数据约定

## 分层

```text
/proc / sysfs / nvidia-smi
            ↓
IResourceCollector → CPU / GPU / Memory Collector
            ↓
SamplingThread → MonitorService（跨线程快照信号）
            ↓
MonitorModel（每个设备最近 60 秒数据）
            ↓
DetailWindow / MiniWindow / HistoryChart / ResourceTile
```

`ApplicationController` 负责服务与界面的连接、持久化和托盘。它不包含任何硬件解析逻辑。

## 任务管理器分支（1.2）

```text
/proc → ProcessCollector → ProcessSamplingThread → ProcessService
                                                      ↓
                                                ProcessController
                                                      ↓
ProcessPage ← ProcessFilterModel ← ProcessTableModel ← ProcessSnapshot
     ↓
身份确认 → ProcessController → ProcessControl → Linux pidfd
```

进程分支使用单独的 `process_backend` 库，不加入 Peony 动态模块。进程身份由 PID、
启动时间和 UID 组成，视图在刷新或排序后仍按身份恢复选择。数据和操作均与 Qt Widgets
解耦，详情对话框及表格只消费快照，不直接读取 procfs 或发送信号。

进程页隐藏、窗口最小化或切到性能页后停止扫描。恢复时重建差值基线，使用 generation
丢弃旧请求的结果；暂停时允许一次手动刷新。窗口退出前唤醒并等待后台线程结束。
进程扫描与原有性能采样独立，详细指标口径和操作保护见 [任务管理器说明](task-manager.md)。

## 网络监控分支（1.3）

```text
QNetworkInterface + sysfs + procfs + nmcli → NetworkCollector
当前用户桌面代理模式 + 环境变量名 → ProxyReader
                     ↓
NetworkSamplingThread → NetworkService → NetworkController → NetworkPage
                                                            ↓
                                    NetworkHistoryChart / NetworkDetailsDialog
```

网络后端使用独立的 `network_backend` 库，只依赖 Qt Core / Network，不依赖 Widgets。
视图消费 `NetworkSnapshot`，格式化、速度卡片、历史曲线和详情窗口各有独立声明与实现文件。
网络页可见时才采样；暂停、隐藏和最小化后停止轮询，恢复后重建速率基线并丢弃旧请求。
网卡身份用接口索引和名称校验；字节差除以实际单调时间，不把缺失或回退计数当成零速率。

DNS 和路由信息缓存 5 秒，接口拓扑变化或手动刷新会使缓存失效。代理查询也缓存 5 秒，
只读取桌面代理模式和本进程继承的环境变量名称，不读取、记录或显示代理认证信息。
命令查询在后台有界等待，不阻塞 GUI，不发起外部测速连接或执行 PAC。
网卡选择与最后页面复用应用的 `QSettings`；运行历史及地址信息不写入用户配置。
具体指标口径与 WSL 范围见 [网络监控说明](network-monitor.md)。

## Peony 原生插件分支（1.1）

```text
同一组 C++ 采集器 → MonitorService → SamplingHub
                                         ↓
Peony WidgetLoader → main.qml → MonitorBackend → ResourceBars（C++ 绘制）
                         ↓
       Widget.config / Widget.actions / Theme / peonyShell
```

原生插件与详细应用复用 `monitor_backend`，不把 QApplication 控制器加载进 Peony。QML 模块 `org.ukui.kylinmonitor` 导出 `MonitorBackend` 与 `ResourceBars`。前者把快照投影为 QML 可用的三组资源，保留缺失值和原因；后者只绘图，不采集硬件。

`SamplingHub` 在 Peony 主线程中管理消费者，所有实例和管理器预览共享一个采样线程，采用最短刷新请求；移除实例取消订阅，没有消费者时暂停。暂停实例保留其最后一次快照。静态服务指针受 `QPointer` 管理，应用退出时停止线程。

桌面元数据位于 `plugin/metadata.json`，使用当前 Peony 的 `ManagerSizes`。管理器与桌面共用 `main.qml`，三档由宿主统一定义。预览只禁止用户副作用，不替换视觉内容，不写入真实实例配置。右键 `Widget.actions` 由宿主创建和弹出菜单；拖动、空位及边缘检查、位置保存、实例删除也由宿主管理。

尺寸切换先调用本机签名 `canResizeWidgetTo(root, width, height)`，检查成功后更新 `variantId` 与实际宽高，保留宿主的网格字段。当前版本的 `canResizeWidgetTo` 同时检查屏幕边界，因此不调用旧模板中本机不存在的额外钳制 API。

`interval`、`paused`、`selectedGpu` 与 `settingsVersion` 通过 `Widget.config` 写入实例配置。框架异步处理写盘事件，因此持久化测试必须等待文件写入再销毁。实时采样历史不是用户编辑内容，不持久化。主动移除实例由宿主删除对应配置；软件包卸载不遍历或清空用户配置。

原生卡片直接使用 UKUI 的 `StyleBackground` 和附加 `Theme` 属性，颜色、边框和深浅切换跟随系统。独立详细窗口使用调色板角色，保留系统 Qt 样式，并监听 UKUI 的主题设置；没有平台主题的会话使用深浅调色板 fallback。

## 接口

`Metric` 的 `std::optional<double>` 将数值和“不可用”明确分开，并带有数据来源或失败原因。`ResourceSample` 通过稳定 ID 标识设备，包含设备类型、名称、利用率、温度、可选容量和扩展属性。`Snapshot` 包含采样时间、设备列表和 WSL 标识。

所有采集器实现 `IResourceCollector::collect()`，返回零个或多个设备。GPU 采集器合并 NVIDIA 与 DRM 设备，避免 NVIDIA 设备重复显示；设备增删会更新侧栏并清理历史数据。

## 采样与退出

后台线程内创建、顺序调用采集器，主线程只接收值类型快照。正常采样间隔扣除实际采集时间。NVIDIA 命令启动等待最多 200 ms、执行等待最多 700 ms，超时终止。单次读取超时显示不可用，不复用陈旧值冒充当前读数。

暂停、修改间隔和退出通过互斥量及条件变量同步。退出先唤醒采样线程并等待采集结束，不强制销毁正在运行的线程。模式切换仅改变窗口显示状态，继续复用同一个采样服务和历史模型。

## 计算与边界

- CPU：只累加 `/proc/stat` 前八项，避免再次计入 guest / guest_nice；idle 与 iowait 为闲置。首帧需要下一次采样。计数器回退、空数据和无时间增量不会制造使用率。
- 内存：优先使用 MemAvailable。旧内核缺少此项时用 MemFree + Buffers + Cached + SReclaimable − Shmem 估算，并在详情中注明。
- 温度：已知 CPU / DIMM 驱动白名单，CPU 的 thermal fallback 只接受明确的 CPU 类型。不会把 NVMe、主板 ACPI 或电池温度当作 CPU / 内存温度。忽略故障、关闭和越界的温度传感器；多个有效值取最大值。
- GPU：NVIDIA CSV 解析支持引号与逗号、多卡、N/A；AMD sysfs 读取 busy、VRAM 和 hwmon。Intel 无通用兼容利用率接口时保留“不支持”，不把频率换算成利用率。
- 历史：按真实时间保留 60 秒，最多 122 个点。缺失数据和长时间暂停造成的断点不会用连线伪造趋势。温度图最低范围 0–120 °C，若读数更高会扩大纵轴。
- 极简：温度柱使用固定 0–120 °C 范围，超过时封顶；悬停仍可见原始读数。0% 的柱是空柱，缺失读数是斜纹柱。

## 扩展

新增平台后端：实现 `IResourceCollector`，复用 `ResourceSample`；在 `SamplingThread` 中注册。新增 GPU 厂商：扩展 GPU 采集器或新增采集器，确保设备 ID 唯一。新增视图：订阅 `MonitorModel::changed()`，不向后端添加 QWidget 依赖。

测试通过 `SourcePaths` 注入临时 proc/sys 目录与命令路径。测试数据只在测试程序中使用，正式应用没有随机数或演示指标。
