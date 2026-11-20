# 网络监控 1.3.0 验证记录

验证日期：2026-11-18。环境为 openKylin 3.0 WSL 2 amd64、Qt 5.15.10、GCC 15.2.0，
包含已安装的 Peony / ukui-quick 原生框架集成测试。

## 构建与测试

Release 编译成功。按后端及宿主、界面及工作流两批执行 CTest，分别 **8/8**、**5/5** 通过，
合计 **13/13 组、96 个功能用例通过**，无失败、无跳过；功能用例不计 QtTest 初始化和清理，
包含数据驱动的各参数组合。两批测试分别耗时 19.86 秒和 2.88 秒。

| 测试组 | 功能用例 | 验证范围 |
| --- | ---: | --- |
| collectors | 11 | CPU / GPU / 内存及资源采样生命周期回归 |
| ui | 5 | 性能窗口、柱状图和文字适配回归 |
| process_backend | 10 | 进程指标、身份校验及自有子进程控制回归 |
| process_service | 6 | 进程后台采样与生命周期回归 |
| process_ui | 11 | 筛选、排序、选择、只读详情、操作确认及窗口适配回归 |
| task_manager_workflow | 1 | 查找、确认并结束测试自身创建的进程 |
| network_collector | 10 | IPv4 / IPv6、掩码、网关、DNS 来源、字节差值、断连及重建、缓存、超时、读取失败 |
| proxy_reader | 12 | 桌面代理模式、未知与超时、环境变量名称、凭据不进入快照、例外规则与 APT 设置区分 |
| network_service | 7 | 隐藏零读取、暂停单次刷新、旧结果丢弃、速率基线、代理缓存、刷新间隔及退出 |
| network_ui | 10 | 网卡选择、菜单保持、未知与零、代理状态、纯文本详情、错误状态、曲线断点及窄窗大字体 |
| network_workflow | 3 | 真实本机 UDP 收发、打开详情和页面切换、网卡与最后页面保存恢复 |
| widget_backend | 5 | 原生插件数据桥、输入事件及主题回归 |
| peony_integration | 5 | 真实框架加载、尺寸、动作、持久化和动态主题回归 |

网络收发工作流仅向本测试创建的 `127.0.0.1` UDP 端口发送 8 个 2048 字节数据报，
由真实接口计数验证上传与下载速率大于零；没有访问外部测速服务。
代理模式及错误分支使用注入的查询结果验证，不改变用户代理设置。
设置恢复测试使用临时配置目录，验证正常退出保存入口和新的应用控制器恢复结果。

复现完整构建及测试：

```bash
bash scripts/build.sh -DBUILD_PEONY_INTEGRATION_TESTS=ON \
    -DUKUI_QUICK_SOURCE_DIR=/path/to/matching/ukui-quick
```

已有匹配的开发包时可省略 `UKUI_QUICK_SOURCE_DIR`；普通功能测试不要求可选的宿主 SDK。
也可以在构建完成后执行 `ctest --test-dir build-openkylin --output-on-failure`。

## 真实截图与窗口检查

运行 `bash scripts/capture-network.sh`，使用实际网络采集服务和实际视图，等待连续采样后抓图。
界面数据来自本机 Linux 接口、sysfs、路由和解析器配置，没有替换为测试夹具或模拟流量。
截图使用隔离的 Xvfb、D-Bus 和配置目录；主题切换不会修改用户的 XRDP 桌面。
截图中的代理状态属于该隔离会话，不用于证明用户桌面或 Windows 宿主的代理开关状态。

| 场景 | 浅色 | 深色 |
| --- | --- | --- |
| 默认窗口 1180×780 | [网络页](previews/network/light/network-1180x780.png) | [网络页](previews/network/dark/network-1180x780.png) |
| 小窗口 760×500 | [网络页](previews/network/light/network-760x500.png) | [网络页](previews/network/dark/network-760x500.png) |
| 详细信息 800×800 | [地址、掩码、网关、DNS](previews/network/light/network-details.png) | [地址、掩码、网关、DNS](previews/network/dark/network-details.png) |

已检查六张实际抓图：默认窗口完整显示操作区、网速、代理和曲线；小窗口优先显示操作、
网速及代理，其他信息可滚动查看。详细信息中 IPv4 / IPv6、子网掩码、网关和 DNS 均可读取。
长文本在详情中换行，支持选择复制；界面自动测试另覆盖 20 pt 字体的窄窗控制可达性。
截图是应用内容抓图，不包含窗口管理器装饰。

## 发布与范围

```bash
bash scripts/package.sh
python3 scripts/prepare-submission.py
```

发布文件为 `dist/kylin-monitor-1.3.0-linux-amd64.deb`、对应源码归档及
`dist/kylin-monitor-1.3.0-MVP-submission.zip`。ZIP 包含完整源码、Debian 包、说明、
截图、GPL 许可证和逐文件 SHA-256 清单；外部校验文件为 `SHA256SUMS-1.3.0.txt`。
旧进程与原生桌面截图保留作为历史验证资料，新网络截图位于独立目录。

网络信息反映 WSL Linux 可见接口，不能推断 Windows 所有网卡或代理设置。
代理显示配置状态，不探测实际请求是否经过代理；DNS 回退来源明确标注为系统全局配置。
没有网关和无法读取路由表分别显示“无”与“未知”；不读取到的指标不以零替代。
本轮重跑了原生框架测试，没有重复完整桌面坐标自动化验收；历史记录见
[原生插件验证记录](verification.md)。

接口、使用方法和模块划分见 [网络监控说明](network-monitor.md) 与 [架构说明](architecture.md)。
