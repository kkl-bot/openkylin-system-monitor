# 窗口与文字适配（1.2.1）

验证环境：openKylin 3.0 WSL 2、Qt 5.15.10、UKUI 浅色和深色主题。
日期：2026-11-18（Asia/Shanghai）。

Release 构建及 8 组 CTest 测试通过，共 54 个功能用例（不计初始化和清理），无跳过。
新增验证覆盖控件换行、长说明及滚动触达、大字体表格、手动列宽保持和详情窗口边界。
真实进程操作回归仍只针对测试自身创建的子进程。

## 调整内容

- 详细窗口最小尺寸为 760×500。较矮窗口通过纵向滚动访问完整内容。
- 性能指标卡根据可用宽度和字体重排；长设备名、说明和容量文本可换行。
- 进程工具栏使用独立 `FlowLayout`，空间不足时换行，不缩小按钮文字。
- 独立 `ProcessTableView` 按字体和原生表头尺寸设置默认列宽、行高；宽窗口把空余空间
  分给文本列，窄窗口支持横向滚动。手动列宽、顺序和排序继续保存，兼容旧配置。
- 资源侧栏及曲线刻度根据字体测量布局；较窄资源卡片将利用率和温度分行。
  侧栏底部环境说明与卡片同属滚动区域，矮窗口或大字体下可滚动查看完整内容。
- 省略的进程名称、数值和设备信息提供完整悬停提示，外部文本经过转义。
- 详情对话框限制在可用屏幕范围内，长命令行可在任意字符间换行。
- 保留系统应用字体；不通过统一缩小文字来容纳内容。

## 复现与截图

```bash
bash scripts/build.sh
bash scripts/capture-layout.sh
```

截图工具运行真实资源和进程采集，等待有效样本后，在隔离的 Xvfb、D-Bus、配置和主题
会话内捕获两套界面。不会写用户配置或发送进程结束信号。

| 窗口 | 浅色 | 深色 |
| --- | --- | --- |
| 760×500 进程 | [查看](previews/window-layout/light/processes-760x500.png) | [查看](previews/window-layout/dark/processes-760x500.png) |
| 1024×720 进程 | [查看](previews/window-layout/light/processes-1024x720.png) | [查看](previews/window-layout/dark/processes-1024x720.png) |
| 1180×780 进程 | [查看](previews/window-layout/light/processes-1180x780.png) | [查看](previews/window-layout/dark/processes-1180x780.png) |
| 760×500 性能 | [查看](previews/window-layout/light/cpu-760x500.png) | [查看](previews/window-layout/dark/cpu-760x500.png) |
| 150% 字体进程 | [查看](previews/window-layout/light/processes-760x500-font150.png) | [查看](previews/window-layout/dark/processes-760x500-font150.png) |
| 150% 字体性能 | [查看](previews/window-layout/light/cpu-760x500-font150.png) | [查看](previews/window-layout/dark/cpu-760x500-font150.png) |

每套主题实际生成 8 张截图，共 16 张。工具输出请求尺寸、实际尺寸、像素尺寸以及表格
实际字号、字体高度；大字体检查若未生效则不保存对应截图。150% 字体属于文字布局压力
验证，不等同于把显示器 DPI 调整为 150%；工具优先设置应用字体，如果平台样式阻止继承，
会按各控件原始字体逐一放大并在输出中明确记录。

本机 UKUI 捕获采用逐控件字体压力方式：表格字号由 10 pt 增至 15 pt，实际字体高度
由 22 增至 32 像素；所有请求的窗口尺寸均保持不变。

窗口小于完整内容时，截图只展示当前滚动位置；其余内容可滚动访问。长名称可能使用省略号，
完整文本可通过悬停和详情查看。测试范围不包含其他桌面环境、多屏或所有屏幕缩放组合。
