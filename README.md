# NAS Touch Display / NAS 触控状态屏

中文：这是一个面向 Waveshare ESP32-S3-Touch-LCD-5B 的 NAS 状态屏项目。硬件端使用 ESP-IDF + LVGL，NAS 端使用 Docker Agent 采集只读系统信息，并通过 HTTP JSON API 提供给 ESP32-S3。

English: This project builds a NAS telemetry touch display for the Waveshare ESP32-S3-Touch-LCD-5B. The firmware uses ESP-IDF + LVGL, while a read-only Docker Agent on the NAS exports telemetry through an HTTP JSON API.

## Current Status / 当前状态

中文：项目已构建通过并烧录到 COM6 的 ESP32-S3-Touch-LCD-5B。固件通过 Wi-Fi 轮询 NAS Docker Agent 的 `/api/v1/status`，以手绘风暖色调 8 页触控界面展示 NAS 状态。Agent 端采用零挂载架构（pid:host + nsenter），无需在极空间上挂载主机卷。

English: The project builds successfully and has been flashed to the ESP32-S3-Touch-LCD-5B on COM6. The firmware polls the NAS Docker Agent `/api/v1/status` over Wi-Fi and renders a hand-drawn warm-tone 8-page touch UI. The Agent uses a zero-mount architecture (pid:host + nsenter), requiring no host volume mounts on ZSpace.

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
  ESP-IDF v6.0.1 + LVGL v9
  Wi-Fi station
  HTTP polling (1 second interval)
  Left/right swipe page navigation (8 pages)
  Header page dots for visual page position
  On-device API host/domain and port input with virtual keyboard
  Hand-drawn warm-tone dashboard

NAS Docker Agent
  FastAPI service
  Read-only host metrics collectors via nsenter
  Zero-mount architecture (pid:host + privileged)
  SMART / NVMe / mdraid / Docker socket adapters
  GET /api/v1/health
  GET /api/v1/status
```

## UI Pages / 界面分页（8 页）

| Page | 中文 | English |
| --- | --- | --- |
| 总览 | NAS 身份、CPU/内存、存储环形图、网络/温度/应用摘要 | NAS identity, CPU/memory, storage ring, network/temp/apps summary |
| 性能 | CPU 环形图 + 负载、内存详情 + Swap、健康标签 | CPU arc + load, memory details + swap, health chips |
| 存储 | 存储池进度条、卷双列卡片 | pool bars, volume dual-column cards |
| 硬盘 | 4×N 槽位网格：温度、容量、坏块、通电时间 | 4×N slot grid: temperature, capacity, bad sectors, power-on hours |
| M.2 | 3×N 芯片卡：容量已用、温度、磨损寿命 | 3×N chip cards: capacity used, temperature, wear |
| 网络 | 总览卡 + 网口卡（状态点 + IP + 错误/丢包） | summary + interface cards (status dot + IP + errors/drops) |
| 应用 | Docker 统计 + 容器双列列表 | Docker stats + container dual-column list |
| 设置 | NAS Agent 地址/端口输入 + 虚拟键盘 + 连接状态 | Agent address/port entry + virtual keyboard + connection status |

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

1. 在 NAS 上使用 `agent/docker-compose.example.yml` 部署 Agent（零挂载架构，默认端口 `8088`，无需 token）。
2. 在 `firmware` 中运行 `idf.py menuconfig`，填写 Wi-Fi SSID 和密码。NAS API 主机名/IP 与端口也可在设备设置页手动输入保存。
3. 构建并烧录：`idf.py -p COM6 build flash`。
4. 设备启动后会轮询 `http://{NAS_IP}:8088/api/v1/status` 并用触控分页显示。

English:

1. Deploy `agent/docker-compose.example.yml` on the NAS (zero-mount architecture, default port `8088`, no token required).
2. Run `idf.py menuconfig` in `firmware` and set Wi-Fi SSID and password. The NAS API hostname/IP and port can also be entered and saved on the device settings page.
3. Build and flash with `idf.py -p COM6 build flash`.
4. The device polls `http://{NAS_IP}:8088/api/v1/status` and presents metrics across 8 touch pages.

## Safety Boundary / 安全边界

中文：NAS 探测和 Agent 设计都以只读为原则。Agent 通过 nsenter 借道宿主机 namespace 读取 `/proc`、`/sys`、挂载点、SMART、mdraid、Docker socket 等信息，不修改 NAS 文件、服务或配置。不挂载任何主机卷。

English: The NAS probe and Agent design are read-only by default. The Agent uses nsenter to read `/proc`, `/sys`, mount points, SMART, mdraid, and Docker socket data from the host namespace without modifying NAS files, services, or configuration. No host volumes are mounted.
