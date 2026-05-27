# ESP32-S3 NAS Touch Display Firmware / ESP32-S3 NAS 触控屏固件

中文：`firmware/` 是 Waveshare ESP32-S3-Touch-LCD-5B 的 ESP-IDF + LVGL 固件。它通过 Wi-Fi 访问 NAS Docker Agent，并以彩色手绘风格的多页触控界面显示 NAS 状态。

English: `firmware/` contains the ESP-IDF + LVGL firmware for the Waveshare ESP32-S3-Touch-LCD-5B. It connects to the NAS Docker Agent over Wi-Fi and renders a colorful hand-drawn multi-page touch dashboard.

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

中文：当前环境已用 ESP-IDF v6.0.1 构建通过。生成文件在 `build/nas_touch_display.bin`。

English: The current environment builds successfully with ESP-IDF v6.0.1. The generated firmware image is `build/nas_touch_display.bin`.

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
  NAS API bearer token
  Status polling interval in milliseconds
```

中文：没有 Wi-Fi SSID 和密码时，固件可以构建，但设备无法连接 NAS。配置完成后再烧录最稳妥。

English: Without Wi-Fi SSID and password, the firmware can build but the device cannot reach the NAS. Flash after configuration for the useful end-to-end test.

中文：设备端“设置”页支持直接输入 NAS Agent 的 IP 或域名，以及端口号。保存后会写入 NVS，后续轮询会使用新地址，无需重新编译固件。

English: The on-device Settings page accepts either a NAS Agent IP address or hostname plus a port. Saving writes the endpoint to NVS, so later polling uses the new address without rebuilding the firmware.

## Flash / 烧录

```powershell
idf.py -p COM6 flash monitor
```

中文：烧录会覆盖 ESP32-S3 当前固件。Docker Agent 需要先在 NAS 上启动，或者设备会显示连接失败/离线状态。

English: Flashing overwrites the current ESP32-S3 firmware. Start the Docker Agent on the NAS first, otherwise the display will show offline/connection failure state.

## UI Pages / 界面分页

| Page | 中文显示内容 | English content |
| --- | --- | --- |
| Home | NAS IP、主机名、运行时间、总体健康、CPU/内存/存储/网络摘要 | NAS IP, hostname, uptime, overall health, CPU/memory/storage/network summary |
| CPU | CPU 占用、负载、温度、内存、缓存、Swap | CPU usage, load, temperature, memory, cache, swap |
| Storage | 存储池、卷容量、已用/剩余、RAID 状态 | pools, volumes, used/free capacity, RAID state |
| Drives | 每块 HDD/SSD 温度、SMART、坏道、通电时间 | HDD/SSD temperature, SMART, bad sectors, power-on hours |
| M.2 | NVMe 容量、温度、寿命/磨损、缓存状态 | NVMe capacity, temperature, wear, cache state |
| Network | 上传/下载速度、网口速率、IP、错误包/丢包 | upload/download speed, link speed, IP, errors/drops |
| Environment | 风扇、UPS、系统告警 | fans, UPS, system alerts |
| Apps | Docker、虚拟机、服务、备份、快照 | Docker, VMs, services, backups, snapshots |
| Settings | 手动输入 API IP/域名、端口、连接状态 | manual API IP/hostname input, port, connection state |

## Implementation Notes / 实现说明

中文：板级初始化使用 Waveshare 5B 官方 RGB/GT911/CH422G 引脚，已适配 ESP-IDF v6 的新 I2C master API。LVGL 使用 PSRAM 绘图缓冲，HTTP 客户端解析 `/api/v1/status` 的共享 JSON 字段。

English: Board bring-up uses the Waveshare 5B RGB/GT911/CH422G pin map and ESP-IDF v6's new I2C master API. LVGL draw buffers live in PSRAM, and the HTTP client parses the shared `/api/v1/status` JSON fields.
