# Hardware Firmware Development / 硬件端开发文档

中文：本文说明 ESP32-S3-Touch-LCD-5B 固件的开发、配置、构建、烧录和调试方式。

English: This document describes firmware development, configuration, build, flashing, and debugging for the ESP32-S3-Touch-LCD-5B.

## Scope / 开发范围

中文：

- 使用 ESP-IDF + LVGL。
- 驱动 Waveshare 5B 的 1024 x 600 RGB LCD。
- 驱动 GT911 电容触控。
- 使用 CH422G 控制触控复位和背光。
- 通过 Wi-Fi 轮询 NAS Docker Agent。
- 支持触控分页，不把所有信息塞在一页。

English:

- Use ESP-IDF + LVGL.
- Drive the Waveshare 5B 1024 x 600 RGB LCD.
- Drive GT911 capacitive touch.
- Use CH422G for touch reset and backlight control.
- Poll the NAS Docker Agent over Wi-Fi.
- Support touch page navigation instead of placing every metric on one page.

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
| `ui.c` | 多页 LVGL 触控界面 | multi-page LVGL touch UI |
| `Kconfig.projbuild` | Wi-Fi 和 NAS API 配置项 | Wi-Fi and NAS API config options |

## UI Design / 界面设计

中文：界面采用彩色手绘风格，但仍保持仪表盘的可读性。每页聚焦一种任务，避免信息拥挤。

English: The UI uses a colorful hand-drawn style while keeping dashboard readability. Each page focuses on one task area to avoid overcrowding.

| Page | 中文 | English |
| --- | --- | --- |
| Home | 总览、NAS 身份、总体健康 | overview, NAS identity, overall health |
| CPU | CPU、负载、温度、内存和 Swap | CPU, load, temperature, memory, swap |
| Storage | 存储池、卷、RAID | pools, volumes, RAID |
| Drives | HDD/SSD SMART、温度、坏道、通电时间 | HDD/SSD SMART, temperature, bad sectors, power-on hours |
| M.2 | NVMe 容量、温度、磨损、缓存状态 | NVMe capacity, temperature, wear, cache state |
| Network | 上传、下载、网口、错误包、丢包 | upload, download, interfaces, errors, drops |
| Environment | 风扇、UPS、告警 | fans, UPS, alerts |
| Apps | Docker、备份、快照、服务状态 | Docker, backup, snapshot, service state |
| Settings | 手动输入 API IP/域名、端口、连接状态 | manual API IP/hostname input, port, connection state |

## Build Steps / 构建步骤

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

中文：当前构建已通过，二进制体积约 `0x1272a0`，使用自定义 6MB factory app 分区还有约 81% 余量。

English: The current build passes. The app binary is about `0x1272a0`, leaving about 81% free in the custom 6MB factory app partition.

## Flash Steps / 烧录步骤

```powershell
cd firmware
. C:\esp\v6.0.1\esp-idf\export.ps1
idf.py -p COM6 flash monitor
```

中文：烧录必须由连接 ESP 的电脑执行。我可以通过当前电脑直接执行，但建议先在 `menuconfig` 中填好 Wi-Fi SSID 和密码。烧录会覆盖 ESP 当前固件。

English: Flashing must be performed from the PC connected to the ESP. I can run it from this machine, but Wi-Fi SSID and password should be configured first. Flashing overwrites the current ESP firmware.

## Runtime Behavior / 运行行为

中文：

- 启动后初始化 LCD 和触控。
- 连接 Wi-Fi。
- 周期性请求 `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status`。
- 解析成功后刷新当前页面。
- 请求失败时显示离线或连接错误状态。
- 设置页可用屏幕键盘输入 IP/域名和端口，并保存到 NVS。

English:

- Initialize LCD and touch after boot.
- Connect to Wi-Fi.
- Periodically request `http://{NAS_HOST_OR_IP}:{PORT}/api/v1/status`.
- Refresh the current page after a successful parse.
- Show offline or connection error state on request failure.
- The Settings page uses an on-screen keyboard to save IP/hostname and port to NVS.

## Next Firmware Improvements / 后续固件增强

中文：

- 增加屏幕上的 Wi-Fi 输入界面，进一步减少依赖 `menuconfig`。
- 增加亮度滑杆和息屏策略。
- 增加 OTA 更新。
- 按新增 UI 文案继续扩展中文子集字体。

English:

- Add on-device Wi-Fi entry to further reduce reliance on `menuconfig`.
- Add brightness control and screen sleep policy.
- Add OTA update.
- Extend the Chinese subset font whenever new UI copy is added.
