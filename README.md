# NAS Touch Display / NAS 触控状态屏

中文：这是一个面向 Waveshare ESP32-S3-Touch-LCD-5B 的 NAS 状态屏项目。硬件端使用 ESP-IDF + LVGL，NAS 端使用 Docker Agent 采集只读系统信息，并通过 HTTP JSON API 提供给 ESP32-S3。

English: This project builds a NAS telemetry touch display for the Waveshare ESP32-S3-Touch-LCD-5B. The firmware uses ESP-IDF + LVGL, while a read-only Docker Agent on the NAS exports telemetry through an HTTP JSON API.

## Current Status / 当前状态

中文：项目代码已经生成，ESP-IDF v6.0.1 固件构建通过，并已烧录到 COM6 上的 ESP32-S3-Touch-LCD-5B。当前固件可以连接 Wi-Fi，NAS Agent 端口暂未提供可用服务时会显示离线/连接失败。

English: The project has been generated. The ESP-IDF v6.0.1 firmware builds successfully and has been flashed to the ESP32-S3-Touch-LCD-5B on COM6. The firmware can connect to Wi-Fi and shows offline/connection failure when the NAS Agent port is not serving data yet.

## Target Hardware / 目标硬件

| Item | Value |
| --- | --- |
| Board / 开发板 | Waveshare ESP32-S3-Touch-LCD-5B |
| MCU / 主控 | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Screen / 屏幕 | 5 inch capacitive touch, 1024 x 600 |
| LCD | 16-bit RGB |
| Touch / 触控 | GT911 over I2C |
| Flash port / 烧录端口 | COM6 |

References / 参考资料：

- [Waveshare ESP32-S3-Touch-LCD-5](https://docs.waveshare.com/ESP32-S3-Touch-LCD-5)
- [Waveshare ESP-IDF tutorial](https://docs.waveshare.com/ESP32-ESP-IDF-Tutorials/ESP-IDF-Installation)

## System Shape / 系统组成

```text
ESP32-S3-Touch-LCD-5B
  ESP-IDF + LVGL
  Wi-Fi station
  HTTP polling
  Left/right swipe page navigation
  On-device API host/domain and port input
  Hand-drawn colorful dashboard

NAS Docker Agent
  FastAPI service
  Read-only host metrics collectors
  SMART / mdraid / hwmon / Docker adapters
  GET /api/v1/health
  GET /api/v1/status
```

## Directory Guide / 目录说明

| Path | 中文 | English |
| --- | --- | --- |
| `agent/` | NAS 端 Docker Agent | NAS-side Docker Agent |
| `firmware/` | ESP-IDF + LVGL 固件 | ESP-IDF + LVGL firmware |
| `docs/ARCHITECTURE.md` | 系统架构 | System architecture |
| `docs/API_CONTRACT.md` | 双端 API 契约 | API contract shared by both sides |
| `docs/HARDWARE_FIRMWARE_DEV.md` | 硬件端开发文档 | Firmware development guide |
| `docs/NAS_DOCKER_AGENT_DEV.md` | Docker 端开发文档 | Docker Agent development guide |
| `docs/ZSPACE_TARGET_NOTES.md` | 极空间实机只读探测记录 | Read-only ZSpace target notes |

## Quick Start / 快速开始

中文：

1. 在 NAS 上部署 `agent/docker-compose.example.yml`，默认端口是 `8088`。
2. 在 `firmware` 中运行 `idf.py menuconfig`，填写 Wi-Fi SSID、Wi-Fi 密码和可选 token。NAS API 主机名/IP 与端口也可以在设备设置页手动输入并保存。
3. 构建并烧录：`idf.py -p COM6 build flash monitor`。
4. 设备启动后会轮询 `http://192.168.101.12:8088/api/v1/status` 并用触控分页显示。

English:

1. Deploy `agent/docker-compose.example.yml` on the NAS. The default HTTP port is `8088`.
2. Run `idf.py menuconfig` in `firmware` and set Wi-Fi SSID, Wi-Fi password, and optional token. The NAS API hostname/IP and port can also be entered and saved on the device settings page.
3. Build and flash with `idf.py -p COM6 build flash monitor`.
4. The device polls `http://192.168.101.12:8088/api/v1/status` and presents metrics across touch pages.

## Safety Boundary / 安全边界

中文：NAS 探测和 Agent 设计都以只读为原则。Agent 读取 `/proc`、`/sys`、挂载点、SMART、mdraid、Docker socket 等信息，不修改 NAS 文件、服务或配置。

English: The NAS probe and Agent design are read-only by default. The Agent reads `/proc`, `/sys`, mount points, SMART, mdraid, and Docker socket data without modifying NAS files, services, or configuration.
