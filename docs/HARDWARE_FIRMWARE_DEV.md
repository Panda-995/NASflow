# Hardware Firmware Development / 硬件端开发文档

中文：本文说明 ESP32-S3-Touch-LCD-5B 固件的开发、配置、构建、烧录和调试方式。

English: This document describes firmware development, configuration, build, flashing, and debugging for the ESP32-S3-Touch-LCD-5B.

## Scope / 开发范围

中文：

- 使用 ESP-IDF v6.0.1 + LVGL v9。
- 驱动 Waveshare 5B 的 1024 x 600 RGB LCD。
- 驱动 GT911 电容触控。
- 使用 CH422G 控制触控复位和背光。
- 通过 Wi-Fi 轮询 NAS Docker Agent。
- 支持触控滑动切页（8 页），Header 页码圆点指示器。
- 设置页支持虚拟键盘输入 NAS Agent 地址和端口。

English:

- Use ESP-IDF v6.0.1 + LVGL v9.
- Drive the Waveshare 5B 1024 x 600 RGB LCD.
- Drive GT911 capacitive touch.
- Use CH422G for touch reset and backlight control.
- Poll the NAS Docker Agent over Wi-Fi.
- Support touch swipe page navigation (8 pages) with header page dots.
- Settings page with virtual keyboard for NAS Agent address and port input.

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
| `app_main.c` | 启动入口，初始化板级、Wi-Fi、UI、轮询任务 | startup entry, initializes board, Wi-Fi, UI, polling task |
| `board_5b.c` | LCD、触控、背光、LVGL tick 和 flush | LCD, touch, backlight, LVGL tick and flush |
| `wifi_manager.c` | Wi-Fi STA 连接管理 | Wi-Fi station connection management |
| `api_client.c` | HTTP GET `/api/v1/status` | HTTP GET `/api/v1/status` |
| `nas_status.c` | JSON 解析和格式化 | JSON parsing and formatting |
| `ui.c` | 8 页手绘风暖色调 LVGL v9 触控界面 | 8-page hand-drawn warm-tone LVGL v9 touch UI |
| `fonts/lv_font_nas_cn_18.c` | 中文子集字体（约 174 字符） | Chinese subset font (~174 glyphs) |
| `Kconfig.projbuild` | Wi-Fi 和 NAS API 配置项 | Wi-Fi and NAS API config options |

## UI Design / 界面设计

中文：界面采用手绘风暖色调设计，基于设计文档 `docs/NAS_Desktop_UI_Design_Document_No_Environment.docx`。共 8 页，Header 带页码圆点指示器，左右滑动切页。无底部导航栏。

English: The UI uses a hand-drawn warm-tone design based on `docs/NAS_Desktop_UI_Design_Document_No_Environment.docx`. 8 pages with header page dots and swipe navigation. No bottom navigation bar.

| Page | 中文 | English |
| --- | --- | --- |
| 总览 | NAS 身份、CPU/内存进度条、存储环形图、网络/温度/应用摘要 | NAS identity, CPU/memory bars, storage ring, network/temp/apps summary |
| 性能 | CPU 环形图 + 1/5/15min 负载、内存详情 + Swap、健康标签 | CPU arc + load, memory + swap, health chips |
| 存储 | 存储池进度条（最多 3 个）、卷双列卡片（最多 4 个） | pool bars (max 3), volume dual-column cards (max 4) |
| 硬盘 | 4×N 槽位网格：盘号、类型、温度、容量、坏块、通电时间 | 4×N slot grid: bay, type, temp, capacity, bad sectors, power-on hours |
| M.2 | 3×N 芯片卡：型号、容量/已用、温度、磨损、缓存状态 | 3×N chip cards: model, capacity/used, temp, wear, cache state |
| 网络 | 总览卡 + 6 网口卡（状态圆点 + IP + 错误/丢包） | summary + 6 interface cards (status dot + IP + errors/drops) |
| 应用 | Docker 运行/停止/异常统计 + 容器双列列表 | Docker running/stopped/unhealthy stats + container dual-column list |
| 设置 | NAS Agent 地址/端口输入 + 虚拟键盘 + 保存按钮 + 连接状态 | Agent address/port entry + virtual keyboard + save + connection status |

### Color System / 配色

| Role | 中文 | Hex |
| --- | --- | --- |
| Background / 背景 | 暖奶油色 | `#FFF8EE` |
| Card / 卡片 | 纯白 | `#FFFFFF` |
| Card border / 卡片边框 | 暖灰线 | `#E8DCCC` |
| Card shadow / 卡片阴影 | 暖棕影 | `#E0D3C2` |
| Primary text / 主文字 | 深蓝灰 | `#2C3E50` |
| Secondary text / 次级文字 | 灰蓝 | `#7F8C8D` |

Page accent colors: 红(总览) / 蓝(性能) / 琥珀(存储) / 绿(硬盘) / 紫(M.2) / 青(网络) / 橙(应用) / 灰(设置)

## Build Steps / 构建步骤

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

中文：当前构建已通过，二进制体积约 `0x12d730`，自定义 6MB factory app 分区还有约 80% 余量。

English: The current build passes. The app binary is about `0x12d730`, leaving about 80% free in the custom 6MB factory app partition.

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
- 连接 Wi-Fi。
- 每 1 秒请求 `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status`。
- 解析成功后刷新当前页面。
- 请求失败时显示离线或连接错误状态。
- 设置页可用虚拟键盘输入 IP/域名和端口，并保存到 NVS。
- 左右滑动切页，Header 圆点指示当前位置。

English:

- Initialize LCD and touch after boot.
- Connect to Wi-Fi.
- Request `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status` every 1 second.
- Refresh the current page after a successful parse.
- Show offline or connection error state on request failure.
- The Settings page uses a virtual keyboard to save IP/hostname and port to NVS.
- Swipe left/right to change pages, header dots indicate current position.

## Next Firmware Improvements / 后续固件增强

中文：

- 增加屏幕上的 Wi-Fi 输入界面，进一步减少依赖 `menuconfig`。
- 增加亮度滑杆和息屏策略。
- 增加 OTA 更新。
- 网络页加入折线图趋势展示。

English:

- Add on-device Wi-Fi entry to further reduce reliance on `menuconfig`.
- Add brightness control and screen sleep policy.
- Add OTA update.
- Add line chart trend display on the Network page.
