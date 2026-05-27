# ESP32-S3 NAS Touch Display Firmware / ESP32-S3 NAS 触控屏固件

中文：`firmware/` 是 Waveshare ESP32-S3-Touch-LCD-5B 的 ESP-IDF v6.0.1 + LVGL v9 固件。它通过 Wi-Fi 访问 NAS Docker Agent，以手绘风暖色调 8 页触控界面显示 NAS 状态。Header 带页码圆点指示器，左右滑动切页，无底部导航栏。

English: `firmware/` contains the ESP-IDF v6.0.1 + LVGL v9 firmware for the Waveshare ESP32-S3-Touch-LCD-5B. It connects to the NAS Docker Agent over Wi-Fi and renders a hand-drawn warm-tone 8-page touch dashboard with header page dots and swipe navigation. No bottom navigation bar.

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

中文：当前环境已用 ESP-IDF v6.0.1 构建通过。生成文件在 `build/nas_touch_display.bin`，二进制约 `0x12d730`（还剩约 80% 空间）。

English: The current environment builds successfully with ESP-IDF v6.0.1. The generated firmware image is `build/nas_touch_display.bin`, approximately `0x12d730` bytes (~80% free).

## Configuration / 配置

Run / 运行：

```powershell
idf.py menuconfig
```

Set / 设置：

```text
NAS Touch Display
  Wi-Fi SSID
  Wi-Fi password
  NAS API host: 192.168.101.12
  NAS API port: 8088
```

中文：没有 Wi-Fi SSID 和密码时，固件可以构建，但设备无法连接 NAS。配置完成后再烧录最稳妥。

English: Without Wi-Fi SSID and password, the firmware can build but the device cannot reach the NAS. Flash after configuration for the useful end-to-end test.

中文：设备端"设置"页支持虚拟键盘直接输入 NAS Agent 的 IP 或域名，以及端口号。保存后会写入 NVS，后续轮询会使用新地址，无需重新编译固件。

English: The on-device Settings page uses a virtual keyboard to accept either a NAS Agent IP address or hostname plus a port. Saving writes the endpoint to NVS, so later polling uses the new address without rebuilding the firmware.

## Flash / 烧录

```powershell
idf.py -p COM6 flash monitor
```

中文：烧录会覆盖 ESP32-S3 当前固件。Docker Agent 需要先在 NAS 上启动，或者设备会显示连接失败/离线状态。

English: Flashing overwrites the current ESP32-S3 firmware. Start the Docker Agent on the NAS first, otherwise the display will show offline/connection failure state.

## UI Pages / 界面分页（8 页）

| Page | 中文显示内容 | English content |
| --- | --- | --- |
| 总览 | NAS 身份、CPU/内存进度条、存储环形图、网络/温度/应用摘要 | NAS identity, CPU/memory bars, storage ring, network/temp/apps summary |
| 性能 | CPU 环形图 + 1/5/15min 负载、内存详情 + Swap、健康标签 | CPU arc + load, memory + swap, health chips |
| 存储 | 存储池进度条（最多 3 个）、卷双列卡片（最多 4 个） | pool bars (max 3), volume dual-column cards (max 4) |
| 硬盘 | 4×N 槽位网格：温度、容量、坏块、通电时间 | 4×N slot grid: temperature, capacity, bad sectors, power-on hours |
| M.2 | 3×N 芯片卡：容量/已用、温度、磨损寿命、缓存状态 | 3×N chip cards: capacity/used, temperature, wear, cache state |
| 网络 | 总览卡 + 网口卡（状态圆点 + IP + 错误/丢包） | summary + interface cards (status dot + IP + errors/drops) |
| 应用 | Docker 运行/停止/异常统计 + 容器双列列表 | Docker running/stopped/unhealthy stats + container dual-column list |
| 设置 | NAS Agent 地址/端口输入 + 虚拟键盘 + 保存 + 连接状态 | Agent address/port entry + virtual keyboard + save + connection status |

## Color System / 配色

| Role | Hex |
| --- | --- |
| Background / 背景 | `#FFF8EE` |
| Card / 卡片 | `#FFFFFF` |
| Card border / 卡片边框 | `#E8DCCC` |
| Card shadow / 卡片阴影 | `#E0D3C2` |
| Primary text / 主文字 | `#2C3E50` |
| Secondary text / 次级文字 | `#7F8C8D` |

Page accent colors: 红(总览) / 蓝(性能) / 琥珀(存储) / 绿(硬盘) / 紫(M.2) / 青(网络) / 橙(应用) / 灰(设置)

## Implementation Notes / 实现说明

中文：板级初始化使用 Waveshare 5B 官方 RGB/GT911/CH422G 引脚，已适配 ESP-IDF v6 的新 I2C master API。LVGL 使用 PSRAM 绘图缓冲，HTTP 客户端解析 `/api/v1/status` 的共享 JSON 字段。中文字体子集约 174 字符，使用 4bpp 位图格式。

English: Board bring-up uses the Waveshare 5B RGB/GT911/CH422G pin map and ESP-IDF v6's new I2C master API. LVGL draw buffers live in PSRAM, and the HTTP client parses the shared `/api/v1/status` JSON fields. The Chinese subset font contains ~174 glyphs in 4bpp bitmap format.
