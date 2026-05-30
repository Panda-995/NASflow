# NASflow Firmware / NASflow 固件

中文：`firmware/` 是 Waveshare ESP32-S3-Touch-LCD-5B 的 ESP-IDF v6.0.1 + LVGL v8.4 固件。它通过 Wi-Fi 访问 NAS Docker Agent，以彩色手绘风 7 页触控界面显示 NAS 状态。Header 带页码圆点指示器、右上角电源状态胶囊，左右滑动切页；NAS 地址、端口、token 和轮询间隔通过 ESP 自带 Web 后台配置。所有页面共用一张 1024×534 原生分辨率背景，由 `image/1.png` 转换为 RGB565 二进制嵌入。

English: `firmware/` contains the ESP-IDF v6.0.1 + LVGL v8.4 firmware for the Waveshare ESP32-S3-Touch-LCD-5B. It connects to the NAS Docker Agent over Wi-Fi and renders a colorful hand-drawn 7-page touch dashboard with header page dots, a top-right power capsule, and swipe navigation. The NAS address, port, token, and polling interval are configured through the ESP-hosted Web backend. All pages share a single 1024×534 native-resolution background converted from `image/1.png` and embedded as an RGB565 binary.

## Hardware / 硬件

| Item | Value |
| --- | --- |
| Resolution / 分辨率 | 1024 x 600 |
| LCD interface / LCD 接口 | 16-bit RGB |
| Touch / 触控 | GT911 I2C |
| IO expander / IO 扩展 | CH422G for touch reset and backlight |
| Serial / 串口 | USB Type-C, COM6 on this PC |

## Build / 构建

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py build
```

中文：当前环境已用 ESP-IDF v6.0.1 构建通过。生成文件在 `build/nasflow.bin`，二进制约 `0x23f090`（~2.4MB，6MB app 分区还剩约 63% 空间）。

English: The current environment builds successfully with ESP-IDF v6.0.1. The generated firmware image is `build/nasflow.bin`, approximately `0x23f090` bytes (~2.4MB, ~63% free in the 6MB app partition).

## Configuration / 配置

Run / 运行：

```powershell
idf.py menuconfig
```

Set / 设置：

```text
NASflow
  Wi-Fi SSID
  Wi-Fi password
  NAS API host: 192.168.101.12
  NAS API port: 8088
  Power telemetry
```

中文：没有 Wi-Fi SSID 和密码时，固件可以构建，但设备无法连接 NAS。配置完成后再烧录最稳妥。

English: Without Wi-Fi SSID and password, the firmware can build but the device cannot reach the NAS. Flash after configuration for the useful end-to-end test.

中文：设备连上 Wi-Fi 后会在"后台"页显示 ESP Web 后台地址。用同一局域网内的浏览器打开 `http://{ESP_IP}/`，可以设置 NAS Agent 的 IP/域名、端口、可选 Bearer token、轮询间隔，也可以重启设备。保存后会写入 NVS，后续轮询会使用新配置，无需重新编译固件。

English: After Wi-Fi connects, the Backend page shows the ESP Web backend URL. Open `http://{ESP_IP}/` from a browser on the same LAN to configure the NAS Agent IP/hostname, port, optional Bearer token, polling interval, or restart the device. Saving writes the settings to NVS, so later polling uses the new configuration without rebuilding the firmware.

## Web Backend / Web 后台

中文：Web 后台运行在 ESP 的 80 端口，包含连接设置、快捷刷新间隔、连接诊断、配置 JSON 和重启入口。`GET /api/config` 返回脱敏配置，`GET /api/test` 由 ESP 直接请求 NAS Agent 的 `/api/v1/health`，连接表单提交到 `POST /config`。token 不会在页面回显；留空表示保留旧值，勾选清除才会删除。

English: The Web backend runs on ESP port 80 and includes connection settings, quick polling presets, connection diagnostics, config JSON access, and restart. `GET /api/config` returns redacted settings, `GET /api/test` makes the ESP call the NAS Agent `/api/v1/health` directly, and the connection form submits to `POST /config`. The token is never echoed back in the page; leaving it empty keeps the previous value, and the clear checkbox removes it.

## Power Telemetry / 电源检测

中文：右上角电源胶囊会显示供电来源、电池百分比和充电状态。默认 Waveshare 5B 原理图只提供电池座、CS8501 充电芯片和板载 CHG/DONE 指示灯，没有把 VBAT、CHG/DONE 或 VBUS 检测信号连接到 ESP32-S3 的可读 GPIO/ADC。因此默认配置下固件会显示“电量 -- 未知”，避免显示假数据。若要启用真实检测，需要额外硬件连接：VBAT 通过安全分压接 ADC GPIO，CHG/DONE 接输入 GPIO，VBUS 或 USB 5V 通过安全分压/电平转换接输入 GPIO，然后在 `menuconfig > NASflow > Power telemetry` 设置引脚、有效电平和分压比例。

English: The top-right power capsule displays power source, battery percentage, and charge state. The default Waveshare 5B schematic provides the battery connector, CS8501 charger, and onboard CHG/DONE LEDs, but does not route VBAT, CHG/DONE, or VBUS sense signals to readable ESP32-S3 GPIO/ADC pins. Therefore the default firmware shows `电量 -- 未知` to avoid fake readings. To enable real telemetry, add hardware sense wiring: VBAT through a safe divider to an ADC GPIO, CHG/DONE to input GPIOs, and VBUS/USB 5V through a safe divider or level shifter to an input GPIO. Then configure pins, active levels, and divider ratio in `menuconfig > NASflow > Power telemetry`.

## Background Asset / 背景资源

中文：项目根目录的 `image/1.png` 会在构建时通过 Python/Pillow 转换为 `firmware/main/bg_page_1.bin`（1024×534 RGB565 二进制），通过 ESP-IDF 的 `EMBED_FILES` 机制嵌入固件。所有 7 个页面共用这一张背景图，运行时直接以原生分辨率显示，无缩放。

English: The project root's `image/1.png` is converted at build time via Python/Pillow into `firmware/main/bg_page_1.bin` (1024×534 RGB565 binary) and embedded into the firmware through ESP-IDF's `EMBED_FILES` mechanism. All 7 pages share this single background image, displayed at native resolution with no runtime scaling.

## Flash / 烧录

```powershell
idf.py -p COM6 flash monitor
```

中文：烧录会覆盖 ESP32-S3 当前固件。Docker Agent 需要先在 NAS 上启动，或者设备会显示连接失败/离线状态。

English: Flashing overwrites the current ESP32-S3 firmware. Start the Docker Agent on the NAS first, otherwise the display will show offline/connection failure state.

## UI Pages / 界面分页（7 页）

| Page | 中文显示内容 | English content |
| --- | --- | --- |
| 总览 | NAS 身份、CPU/内存进度条、存储环形图、网络/温度/应用摘要 | NAS identity, CPU/memory bars, storage ring, network/temp/apps summary |
| 性能 | CPU 环形图 + 1/5/15min 负载、内存详情 + Swap、健康标签 | CPU arc + load, memory + swap, health chips |
| 存储 | 所有存储池合计总容量/已用/剩余、单池健康摘要 | combined total/used/free capacity across all pools, per-pool health summary |
| 硬盘 | HDD/SSD 与 M.2/NVMe 卡片：温度、容量、坏块、通电时间、磨损 | HDD/SSD and M.2/NVMe cards: temperature, capacity, bad sectors, power-on hours, wear |
| 网络 | 上传下载、已接入网口数量、每个网口 IP/速率/错误/丢包 | upload/download, connected NIC count, per-interface IP/speed/errors/drops |
| 服务 | Docker 运行/停止/异常统计 + 容器列表 | Docker running/stopped/unhealthy stats + container list |
| 后台 | ESP Web 后台地址、当前 NAS 目标、连接状态 | ESP Web backend URL, current NAS target, connection state |

## Color System / 配色

| Role | Hex |
| --- | --- |
| Background / 背景 | `#FFF8EE` |
| Card / 卡片 | `#FFFFFF` |
| Card border / 卡片边框 | `#E8DCCC` |
| Card shadow / 卡片阴影 | `#E0D3C2` |
| Primary text / 主文字 | `#2C3E50` |
| Secondary text / 次级文字 | `#7F8C8D` |

Page accent colors: 珊瑚红(总览) / 青绿(性能) / 琥珀(存储) / 蓝紫(硬盘) / 绿色(网络) / 紫色(服务) / 天蓝(后台)

## Implementation Notes / 实现说明

中文：板级初始化使用 Waveshare 5B 官方 RGB/GT911/CH422G 引脚，已适配 ESP-IDF v6 的新 I2C master API。LVGL 使用 PSRAM 绘图缓冲，HTTP 客户端解析 `/api/v1/status` 的共享 JSON 字段。中文界面使用未压缩的 Noto Sans SC 子集位图字体，以减少乱码和缺字。背景图以 1024×534 RGB565 二进制通过 ESP-IDF `EMBED_FILES` 嵌入固件，运行时原生分辨率显示。`power_monitor.c` 默认只显示未知；配置了可读采样引脚后才输出真实电源状态。

English: Board bring-up uses the Waveshare 5B RGB/GT911/CH422G pin map and ESP-IDF v6's new I2C master API. LVGL draw buffers live in PSRAM, and the HTTP client parses the shared `/api/v1/status` JSON fields. The Chinese UI uses an uncompressed Noto Sans SC subset bitmap font to reduce mojibake and missing glyphs. The background is embedded as a 1024×534 RGB565 binary via ESP-IDF `EMBED_FILES` and displayed at native resolution. `power_monitor.c` shows unknown by default and only reports real power telemetry when readable sense pins are configured.
