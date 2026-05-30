# Hardware Firmware Development / 硬件端开发文档

中文：本文说明 ESP32-S3-Touch-LCD-5B 固件的开发、配置、构建、烧录和调试方式。

English: This document describes firmware development, configuration, build, flashing, and debugging for the ESP32-S3-Touch-LCD-5B.

## Scope / 开发范围

中文：

- 使用 ESP-IDF v6.0.1 + LVGL v8.4。
- 驱动 Waveshare 5B 的 1024 x 600 RGB LCD。
- 驱动 GT911 电容触控。
- 使用 CH422G 控制触控复位和背光。
- 通过 Wi-Fi 轮询 NAS Docker Agent。
- 支持触控左右滑动切页（7 页），Header 页码圆点指示器。
- Header 右上角显示电源状态胶囊；默认板级信号不可读时显示未知，外接采样线后可显示电池百分比、充电状态和供电来源。
- 屏幕"后台"页显示 ESP Web 后台地址，NAS Agent 地址和端口在浏览器后台配置。

English:

- Use ESP-IDF v6.0.1 + LVGL v8.4.
- Drive the Waveshare 5B 1024 x 600 RGB LCD.
- Drive GT911 capacitive touch.
- Use CH422G for touch reset and backlight control.
- Poll the NAS Docker Agent over Wi-Fi.
- Support left/right touch swipe page navigation (7 pages) with header page dots.
- Show a top-right power capsule in the header; it shows unknown on the default unreadable board signals, and can show battery percentage, charge state, and power source after external sense wiring is added.
- The screen's Backend page shows the ESP Web backend URL; NAS Agent address and port are configured in the browser backend.

## Board Signals / 板级信号

中文：引脚来自 Waveshare 官方 5B 示例，已写入 `firmware/main/board_5b.c`。

English: Pins are taken from the official Waveshare 5B example and are implemented in `firmware/main/board_5b.c`.

| Signal | GPIO |
| --- | --- |
| VSYNC | 3 |
| HSYNC | 46 |
| DE | 5 |
| PCLK | 7 |
| RGB D0-D15 | 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 |
| I2C SDA | 8 |
| I2C SCL | 9 |
| Touch IRQ | 4 |
| Touch controller | GT911 at `0x5D` |
| IO expander | CH422G at `0x24` and `0x38` |

References / 参考资料：

- [Waveshare ESP32-S3-Touch-LCD-5](https://docs.waveshare.com/ESP32-S3-Touch-LCD-5)
- [Waveshare ESP-IDF tutorial](https://docs.waveshare.com/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation)

## Firmware Modules / 固件模块

| File | 中文 | English |
| --- | --- | --- |
| `app_main.c` | 启动入口，初始化板级、Wi-Fi、UI、Web 后台、轮询任务 | startup entry, initializes board, Wi-Fi, UI, Web backend, polling task |
| `board_5b.c` | LCD、触控、背光、LVGL tick 和 flush | LCD, touch, backlight, LVGL tick and flush |
| `backgrounds/` | 由 `image/1.png` 生成的 1024×534 RGB565 嵌入式背景（二进制内嵌） | single 1024×534 RGB565 background embedded as binary from `image/1.png` |
| `wifi_manager.c` | Wi-Fi STA 连接管理 | Wi-Fi station connection management |
| `api_client.c` | HTTP GET `/api/v1/status` | HTTP GET `/api/v1/status` |
| `web_server.c` | ESP 端配置后台，支持配置 API、连接测试、重启 | ESP-side settings backend with config API, connection test, restart |
| `nas_status.c` | JSON 解析和格式化 | JSON parsing and formatting |
| `power_monitor.c` | 可选电池/充电/USB 供电检测；未配置引脚时输出未知 | optional battery/charge/USB power detection; reports unknown when pins are not configured |
| `ui.c` | 7 页彩色手绘风 LVGL v8.4 触控界面 | 7-page colorful hand-drawn LVGL v8.4 touch UI |
| `fonts/lv_font_nas_cn_18.c` | 未压缩 Noto Sans SC 中文子集字体 | Uncompressed Noto Sans SC Chinese subset font |
| `Kconfig.projbuild` | Wi-Fi 和 NAS API 配置项 | Wi-Fi and NAS API config options |

## UI Design / 界面设计

中文：界面采用彩色手绘风桌面摆件设计，基于设计文档和本轮 UI/UX 梳理。共 7 页，Header 带页码圆点指示器，左右滑动切页。设计重点是降低单页信息密度、突出关键数值、用色块和纸片感卡片建立摆件属性。所有页面共用一张 1024×534 原生分辨率背景，由 `image/1.png` 通过 Pillow 转换为 RGB565 二进制嵌入固件，无运行时缩放。配置不再塞进屏幕虚拟键盘，而是放到 ESP Web 后台。

English: The UI uses a colorful hand-drawn desktop-ornament design based on this round of UI/UX review. It has 7 pages with header page dots and swipe navigation. The design reduces per-page density, prioritizes key values, and uses colored rails plus paper-like cards to improve the ornament feel. All pages share a single 1024×534 native-resolution background converted from `image/1.png` to an RGB565 binary via Pillow and embedded in the firmware with no runtime scaling. Configuration is no longer squeezed into an on-screen keyboard; it lives in the ESP Web backend.

| Page | 中文 | English |
| --- | --- | --- |
| 总览 | NAS 身份、CPU/内存进度条、存储环形图、网络/温度/应用摘要 | NAS identity, CPU/memory bars, storage ring, network/temp/apps summary |
| 性能 | CPU 环形图 + 1/5/15min 负载、内存详情 + Swap、健康标签 | CPU arc + load, memory + swap, health chips |
| 存储 | 所有存储池合计总容量/已用/剩余、单池健康摘要 | combined total/used/free capacity across all pools, per-pool health summary |
| 硬盘 | HDD/SSD 与 M.2/NVMe 卡片：盘号/槽位、温度、容量、坏块、通电时间、磨损 | HDD/SSD and M.2/NVMe cards: bay/slot, temp, capacity, bad sectors, power-on hours, wear |
| 网络 | 上传下载、已接入网口数量、6 个网口卡（IP + 速率 + 错误/丢包） | upload/download, connected NIC count, 6 interface cards (IP + speed + errors/drops) |
| 服务 | Docker 运行/停止/异常统计 + 容器列表 | Docker running/stopped/unhealthy stats + container list |
| 后台 | ESP Web 后台地址、当前 NAS 目标、连接状态 | ESP Web backend URL, current NAS target, connection state |

### UI/UX Decisions / UI/UX 决策

中文：

- 信息架构：屏幕只展示，不承担复杂输入；复杂设置迁移到 Web 后台。
- 视觉层级：每页 1 个主卡 + 少量辅卡，避免把所有 NAS 信息塞满一屏。
- 存储页：主卡显示所有存储池合计，避免用户误读单池容量为总容量。
- 网络页：网口卡直接显示 IP，已接入数量放在顶部摘要。
- 服务页：只展示 Docker，并加入容器列表，备份/快照不再混入。
- 背景系统：图片作为桌面摆件的情绪层，数据卡片保持半透明纸片感，保证可读性。
- 电源状态：右上角只展示真实可读数据；默认硬件没有把 VBAT/CHG/DONE/VBUS 接到 ESP 时显示未知。

English:

- Information architecture: the screen displays; complex input belongs in the Web backend.
- Visual hierarchy: each page uses one primary card plus a few secondary cards instead of filling the whole screen.
- Storage page: the primary card shows combined totals across all pools to avoid mistaking one pool for total capacity.
- Network page: interface cards show IP addresses directly, with connected count in the top summary.
- Services page: Docker only, with a container list; backup/snapshot data is no longer mixed in.
- Background system: images provide the ornament-like visual layer, while semi-opaque paper cards preserve readability.
- Power status: the top-right capsule only shows readable data; it shows unknown when the default hardware does not route VBAT/CHG/DONE/VBUS to the ESP.

## Power Telemetry / 电源检测

中文：Waveshare ESP32-S3-Touch-LCD-5B 有 3.7V 电池座、CS8501 充电芯片和板载 CHG/DONE LED，但默认原理图没有把这些检测点接到 ESP32-S3 的 ADC/GPIO。固件因此采用“可配置但不造假”的策略：

- 默认 `NAS_DISPLAY_BATTERY_ADC_GPIO=-1`，不显示电池百分比。
- 默认 `NAS_DISPLAY_CHARGE_GPIO=-1` 和 `NAS_DISPLAY_DONE_GPIO=-1`，不显示真实充电/已满状态。
- 默认 `NAS_DISPLAY_USB_DETECT_GPIO=-1`，不判断 USB 数据线供电还是电池供电。
- UI 右上角会显示 `电量 -- 未知`。
- 若用户后续焊接采样线，可在 `idf.py menuconfig > NASflow > Power telemetry` 配置 ADC GPIO、分压比例、CHG/DONE GPIO、VBUS 检测 GPIO 和有效电平。

English: The Waveshare ESP32-S3-Touch-LCD-5B has a 3.7V battery connector, CS8501 charger, and onboard CHG/DONE LEDs, but the default schematic does not route these sense points to ESP32-S3 ADC/GPIO pins. The firmware therefore follows a "configurable but no fake data" strategy:

- `NAS_DISPLAY_BATTERY_ADC_GPIO=-1` by default, so battery percentage is not shown.
- `NAS_DISPLAY_CHARGE_GPIO=-1` and `NAS_DISPLAY_DONE_GPIO=-1` by default, so real charging/full state is not shown.
- `NAS_DISPLAY_USB_DETECT_GPIO=-1` by default, so USB data-cable power versus battery power is not inferred.
- The top-right UI shows `电量 -- 未知`.
- If sense wires are later soldered, configure ADC GPIO, divider ratio, CHG/DONE GPIOs, VBUS-detect GPIO, and active levels in `idf.py menuconfig > NASflow > Power telemetry`.

### Color System / 配色

| Role | 中文 | Hex |
| --- | --- | --- |
| Background / 背景 | 暖奶油色 | `#FFF8EE` |
| Card / 卡片 | 纯白 | `#FFFFFF` |
| Card border / 卡片边框 | 暖灰线 | `#E8DCCC` |
| Card shadow / 卡片阴影 | 暖棕影 | `#E0D3C2` |
| Primary text / 主文字 | 深蓝灰 | `#2C3E50` |
| Secondary text / 次级文字 | 灰蓝 | `#7F8C8D` |

Page accent colors: 珊瑚红(总览) / 青绿(性能) / 琥珀(存储) / 蓝紫(硬盘) / 绿色(网络) / 紫色(服务) / 天蓝(后台)

## Build Steps / 构建步骤

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

中文：当前构建已通过，二进制体积约 `0x23f090`（~2.4MB），自定义 6MB factory app 分区还有约 63% 余量。

English: The current build passes. The app binary is about `0x23f090` (~2.4MB), leaving about 63% free in the custom 6MB factory app partition.

## Flash Steps / 烧录步骤

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py -p COM6 flash monitor
```

中文：烧录必须由连接 ESP 的电脑执行。建议先在 `menuconfig` 中填好 Wi-Fi SSID 和密码。烧录会覆盖 ESP 当前固件。

English: Flashing must be performed from the PC connected to the ESP. Wi-Fi SSID and password should be configured first in `menuconfig`. Flashing overwrites the current ESP firmware.

## Runtime Behavior / 运行行为

中文：

- 启动后初始化 LCD 和触控。
- 连接 Wi-Fi，并在 80 端口启动 ESP Web 后台。
- 屏幕“后台”页显示 `http://{ESP_IP}`。
- 按配置的轮询间隔请求 `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status`。
- 解析成功后刷新当前页面。
- 请求失败时显示离线或连接错误状态。
- 右上角刷新电源状态；默认无采样引脚时保持未知。
- Web 后台可保存 IP/域名、端口、可选 token 和轮询间隔到 NVS，并通过 `/api/test` 测试 NAS Agent 健康接口。
- 左右滑动切页，Header 圆点指示当前位置。

English:

- Initialize LCD and touch after boot.
- Connect to Wi-Fi and start the ESP Web backend on port 80.
- Show `http://{ESP_IP}` on the screen's Backend page.
- Request `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status` at the configured polling interval.
- Refresh the current page after a successful parse.
- Show offline or connection error state on request failure.
- Refresh the top-right power status; it remains unknown when no sense pins are configured.
- The Web backend saves IP/hostname, port, optional token, and polling interval to NVS, and tests the NAS Agent health endpoint through `/api/test`.
- Swipe left/right to change pages, header dots indicate current position.

## Next Firmware Improvements / 后续固件增强

中文：

- 增加 Web 后台中的 Wi-Fi 配网流程，进一步减少依赖 `menuconfig`。
- 增加亮度滑杆和息屏策略。
- 增加 OTA 更新。
- 网络页加入折线图趋势展示。

English:

- Add Wi-Fi provisioning to the Web backend to further reduce reliance on `menuconfig`.
- Add brightness control and screen sleep policy.
- Add OTA update.
- Add line chart trend display on the Network page.
