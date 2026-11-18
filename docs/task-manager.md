# 任务管理器扩展（1.2）

详细窗口增加“进程”页，左侧的 CPU、GPU 和内存条目仍打开性能曲线。
使用 `/usr/bin/kylin-monitor --processes` 可直接进入进程页。首次启动默认显示进程，
正常退出时保存最后访问的页面及进程表格列宽、顺序和排序。

## 进程列表

列表包含名称、PID、用户、状态、CPU、常驻内存、内存比例、线程数及磁盘读写速率。
点击表头按数值排序，搜索名称、PID 或用户；可仅查看自己的进程。
选中一行查看详情，可读到父 PID、命令行和可执行文件位置。

CPU 列表示占本机全部逻辑 CPU 总容量的比例，与性能页的 0–100% 口径一致。
初次采样、计数器回退或进程重建时没有差值，显示“—”，待后续采样产生有效读数。
内存使用 RSS（常驻集），共享页会计入多个进程，不能把该列总和当作系统已用内存。
磁盘列根据 `/proc/<pid>/io` 的 `read_bytes` / `write_bytes` 计算每秒速率，
权限不足时显示“—”，不代表进程没有 I/O；这些数值不是网络速度。

进程采样在单独线程中完成。进程页不可见时停止扫描；重新显示后重新建立差值基线。
暂停后保留表格，手动刷新可以取得一次新快照。刷新频率与性能页面、备用悬浮窗同步。
进程清单和命令行只保存在内存中，不写入用户配置或上传。

## 结束进程

“结束进程”发送 SIGTERM，让程序有机会清理退出；“强制结束”发送 SIGKILL。
两种操作都先显示名称和 PID 确认，默认取消。结束请求不等于已经退出，列表以后续
真实采样为准；例如程序可以忽略 SIGTERM，部分内核等待状态可能不会立即退出。

仅允许对当前普通用户符合条件的进程操作，不自动提权；root 进程、PID 1、任务管理器自身和不属于
当前用户的进程不可操作；以 root 运行也仅提供只读监测。终止前重新核对 UID、PID 和启动时间，并通过 Linux pidfd
指向具体进程，避免进程退出后同一 PID 被复用时误伤另一进程。系统缺少 pidfd 支持时
给出原因，不退回仅凭 PID 发送信号的方式。

暂停列表中的目标或确认框停留期间的目标也需重新验证。当前阶段提供单进程管理，
不包含进程树批量结束、服务管理、开机启动管理或 Windows 宿主进程管理。

## 新增模块

| 位置 | 职责 |
| --- | --- |
| `src/domain/process_types.*` | 进程身份和快照值类型 |
| `src/backend/process_collector.*` | Linux procfs 解析与差值计算 |
| `src/backend/process_control.*` | 权限、身份检查与 pidfd 操作 |
| `src/backend/process_sampling_thread.*` | 后台采样、等待、恢复基线 |
| `src/backend/process_service.*` | 主线程信号接口及过期快照过滤 |
| `src/ui/process_table_model.*` | 表格数据及数值排序角色 |
| `src/ui/process_filter_model.*` | 搜索与用户过滤 |
| `src/ui/process_page.*` | 进程页、选择保持和动作确认 |
| `src/ui/process_details_dialog.*` | 只读详情对话框 |
| `src/ui/process_presentation.*` | 表格与详情共用的中文状态转换 |
| `src/app/process_controller.*` | 采样、进程操作与界面接线 |

原有 `DetailWindow` 仅增加页面导航，`ApplicationController` 负责少量连接与设置保存。
Peony 动态模块继续只链接资源采集后端，进程管理库只在独立任务管理器中使用。

接口依据：[Linux procfs 文档](https://docs.kernel.org/filesystems/proc.html)、
[pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html)、
[pidfd_send_signal](https://man7.org/linux/man-pages/man2/pidfd_send_signal.2.html)。
