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
- 屏幕“后台”页显示 ESP Web 后台地址，NAS Agent 地址、端口和页面背景在浏览器后台配置。

English:

- Use ESP-IDF v6.0.1 + LVGL v8.4.
- Drive the Waveshare 5B 1024 x 600 RGB LCD.
- Drive GT911 capacitive touch.
- Use CH422G for touch reset and backlight control.
- Poll the NAS Docker Agent over Wi-Fi.
- Support left/right touch swipe page navigation (7 pages) with header page dots.
- The screen's Backend page shows the ESP Web backend URL; NAS Agent address, port, and page backgrounds are configured in the browser backend.

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
| `backgrounds/` | 由 `image/1.png` 到 `image/7.png` 生成的内置 RGB565 背景 | embedded RGB565 backgrounds generated from `image/1.png` through `image/7.png` |
| `wifi_manager.c` | Wi-Fi STA 连接管理 | Wi-Fi station connection management |
| `api_client.c` | HTTP GET `/api/v1/status` | HTTP GET `/api/v1/status` |
| `web_server.c` | ESP 端配置后台，支持页面、配置 API、连接测试、重启 | ESP-side settings backend with page, config API, connection test, restart |
| `nas_status.c` | JSON 解析和格式化 | JSON parsing and formatting |
| `ui.c` | 7 页彩色手绘风 LVGL v8.4 触控界面 | 7-page colorful hand-drawn LVGL v8.4 touch UI |
| `fonts/lv_font_nas_cn_18.c` | 未压缩 Noto Sans SC 中文子集字体 | Uncompressed Noto Sans SC Chinese subset font |
| `Kconfig.projbuild` | Wi-Fi 和 NAS API 配置项 | Wi-Fi and NAS API config options |

## UI Design / 界面设计

中文：界面采用彩色手绘风桌面摆件设计，基于设计文档 `docs/NAS_Desktop_UI_Design_Document_No_Environment.docx` 和本轮 UI/UX 梳理。共 7 页，Header 带页码圆点指示器，左右滑动切页。设计重点是降低单页信息密度、突出关键数值、用色块和纸片感卡片建立摆件属性。每页背景来自 `image/1.png` 到 `image/7.png` 的内置资源，Web 后台可调整页面与背景的映射。配置不再塞进屏幕虚拟键盘，而是放到 ESP Web 后台。

English: The UI uses a colorful hand-drawn desktop-ornament design based on `docs/NAS_Desktop_UI_Design_Document_No_Environment.docx` and this round of UI/UX review. It has 7 pages with header page dots and swipe navigation. The design reduces per-page density, prioritizes key values, and uses colored rails plus paper-like cards to improve the ornament feel. Each page background comes from the embedded assets generated from `image/1.png` through `image/7.png`, and the Web backend can remap backgrounds per page. Configuration is no longer squeezed into an on-screen keyboard; it lives in the ESP Web backend.

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

English:

- Information architecture: the screen displays; complex input belongs in the Web backend.
- Visual hierarchy: each page uses one primary card plus a few secondary cards instead of filling the whole screen.
- Storage page: the primary card shows combined totals across all pools to avoid mistaking one pool for total capacity.
- Network page: interface cards show IP addresses directly, with connected count in the top summary.
- Services page: Docker only, with a container list; backup/snapshot data is no longer mixed in.
- Background system: images provide the ornament-like visual layer, while semi-opaque paper cards preserve readability.

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

中文：当前构建已通过，二进制体积约 `0x1a64a0`，自定义 6MB factory app 分区还有约 73% 余量。

English: The current build passes. The app binary is about `0x1a64a0`, leaving about 73% free in the custom 6MB factory app partition.

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
- Web 后台可保存 IP/域名、端口、可选 token、轮询间隔、页面背景映射到 NVS，并通过 `/api/test` 测试 NAS Agent 健康接口。
- 左右滑动切页，Header 圆点指示当前位置。

English:

- Initialize LCD and touch after boot.
- Connect to Wi-Fi and start the ESP Web backend on port 80.
- Show `http://{ESP_IP}` on the screen's Backend page.
- Request `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status` at the configured polling interval.
- Refresh the current page after a successful parse.
- Show offline or connection error state on request failure.
- The Web backend saves IP/hostname, port, optional token, polling interval, and page-background mapping to NVS, and tests the NAS Agent health endpoint through `/api/test`.
- Swipe left/right to change pages, header dots indicate current position.

## Next Firmware Improvements / 后续固件增强

中文：

- 增加 Web 后台中的 Wi-Fi 配网流程，进一步减少依赖 `menuconfig`。
- 增加亮度滑杆和息屏策略。
- 增加 OTA 更新。
- 网络页加入折线图趋势展示。
- 如需真正上传自定义背景图，可新增文件系统分区和 PNG/JPEG 解码器。

English:

- Add Wi-Fi provisioning to the Web backend to further reduce reliance on `menuconfig`.
- Add brightness control and screen sleep policy.
- Add OTA update.
- Add line chart trend display on the Network page.
- To support true custom background uploads, add a filesystem partition plus a PNG/JPEG decoder.
