# ZSpace Target Notes / 极空间目标环境记录

中文：本文件记录对目标极空间 NAS 的只读探测结果，用来让 Docker Agent 和 ESP32 固件默认配置贴合真实环境。探测过程只读取必要信息，没有修改、编辑或移动 NAS 上的内容。

English: This file records read-only observations from the target ZSpace NAS so the Docker Agent and ESP32 firmware defaults match the real environment. The probe only read necessary data and did not modify, edit, or move NAS content.

## Connection / 连接信息

| Item | Value |
| --- | --- |
| NAS LAN IP / NAS 局域网 IP | `192.168.101.12` |
| Planned Agent port / 计划 Agent 端口 | `8088` |
| ESP serial port / ESP 串口 | `COM6` |

## System / 系统

| Item | Value |
| --- | --- |
| Hostname / 主机名 | `Z423-8OU2` |
| OS / 系统 | ZOS, Debian-like Linux |
| ZOS version / ZOS 版本 | `V1.0.1400212` |
| Kernel / 内核 | `6.8.1-z423-generic` |
| CPU | AMD Ryzen 7 5825U with Radeon Graphics |
| CPU threads / CPU 线程 | 16 |
| Memory / 内存 | about 32 GB |
| Main physical NIC / 主物理网口 | `eth1` |
| Link speed / 链路速率 | 2500 Mbps |

## Storage / 存储

Observed mounts / 已观察到的挂载点：

| Mount | Filesystem | 中文说明 | English notes |
| --- | --- | --- | --- |
| `/data_s001` | btrfs | HDD 数据卷 | HDD data volume |
| `/data_s002` | btrfs | HDD 数据卷 | HDD data volume |
| `/data_n003` | btrfs | NVMe 数据卷和 Docker 区域 | NVMe data volume and Docker area |
| `/zspace` | ext4 | ZOS 系统/应用区域 | ZOS system/application area |
| `/zspace/zsrp` | ext4 on mdraid | ZOS 保护区域 | ZOS protected area |

Observed devices / 已观察到的设备：

| Device | Type | 中文说明 | English notes |
| --- | --- | --- | --- |
| `/dev/sda` | SATA HDD | SMART 可读 | SMART available |
| `/dev/sdb` | SATA HDD | SMART 可读 | SMART available |
| `/dev/nvme0n1` | NVMe | 可通过 `smartctl` 读取健康、温度、磨损 | health, temperature, wear via `smartctl` |
| `/dev/nvme1n1` | NVMe | 可通过 `smartctl` 读取健康、温度、磨损 | health, temperature, wear via `smartctl` |
| `/dev/nvme2n1` | NVMe | 可通过 `smartctl` 读取健康、温度、磨损 | health, temperature, wear via `smartctl` |
| `/dev/md0` | mdraid raid1 | 状态 clean | clean |
| `/dev/md1` | mdraid raid1 | 状态 clean | clean |

## Available Data For ESP / 目前可提供给 ESP 的信息

中文：以下信息已经能通过通用 Linux/Docker/SMART 方式提供给 ESP，Agent 会统一输出到 `/api/v1/status`。

English: The following data can already be exposed to the ESP through generic Linux, Docker, and SMART collectors. The Agent normalizes it into `/api/v1/status`.

| Category | 中文 | English |
| --- | --- | --- |
| NAS identity | IP、主机名、ZOS 版本、内核、运行时间、总体健康 | IP, hostname, ZOS version, kernel, uptime, overall health |
| CPU | 占用率、1/5/15 分钟负载、CPU 温度、线程数 | usage, 1/5/15 min load, CPU temperature, thread count |
| Memory | 总量、已用、可用、缓存、Swap 总量和已用 | total, used, available, cache, swap total and used |
| Storage | `/data_s001`、`/data_s002`、`/data_n003`、`/zspace` 容量和已用/剩余 | capacity and used/free space for `/data_s001`, `/data_s002`, `/data_n003`, `/zspace` |
| RAID | `/dev/md0`、`/dev/md1` mdraid 状态，当前 clean | mdraid status for `/dev/md0` and `/dev/md1`, currently clean |
| HDD/SSD | `/dev/sda`、`/dev/sdb` 型号、容量、SMART、温度、坏道、通电时间 | model, capacity, SMART, temperature, bad sectors, power-on hours for `/dev/sda`, `/dev/sdb` |
| NVMe | 三块 NVMe 容量、温度、SMART、available spare、percentage used/wear | capacity, temperature, SMART, available spare, percentage used/wear for three NVMe drives |
| Network | `eth1` 速率 2500 Mbps、IP、上传/下载速度、错误包、丢包 | `eth1` 2500 Mbps link, IP, upload/download speed, errors, drops |
| Sensors | CPU、NVMe、eth1、GPU 等温度 | CPU, NVMe, eth1, GPU temperatures |
| Docker | 容器数量、运行状态、health 状态；已观察到 healthy/unhealthy 容器 | container count, running state, health state; healthy/unhealthy containers observed |

## Limited Or Missing / 受限或暂未稳定暴露

| Field | 中文说明 | English notes |
| --- | --- | --- |
| Fan RPM | `hwmon` 探测到温度，但未看到 `fan*_input`，因此风扇转速可能返回空数组 | `hwmon` exposed temperatures but no `fan*_input`; fan RPM may be empty |
| UPS | 系统有 `upsc` 命令，但未确认 NUT/UPS 已配置 | `upsc` exists, but NUT/UPS configuration is not confirmed |
| Backup status | 需要极空间 ZOS 专有接口或用户提供任务元数据；当前返回空数组 | needs a ZOS-specific adapter or user-provided task metadata; currently returns empty arrays |
| Snapshot status | 需要 ZOS 专有接口或可读快照目录规则；当前返回空数组 | needs a ZOS-specific adapter or readable snapshot rules; currently returns empty arrays |
| VM status | 已看到虚拟网桥痕迹，但未实现安全只读 VM 适配器 | virtual bridges were observed, but a safe read-only VM adapter is not implemented yet |
| Native ZOS alerts | 暂未找到稳定只读来源；当前用采集器健康状态生成告警 | no stable read-only source found yet; current alerts are derived from collector health |

## Host Tools / 主机工具

Available / 已存在：

```text
docker
smartctl
mdadm
btrfs
sensors
upsc
lsblk
dmsetup
ip
ethtool
```

Not observed / 未观察到：

```text
nvme
zpool
zfs
```

## Collector Decisions / 采集决策

中文：

- Docker Agent 使用 `network_mode: host`，避免容器网桥影响 IP 和流量数据。
- 数据卷、系统目录、设备目录均以只读挂载为主。
- SATA 和 NVMe 都优先使用 `smartctl`。
- 无法采集的数据返回 `null`、`unknown` 或空数组，不让整个 API 失败。

English:

- The Docker Agent uses `network_mode: host` so IP and traffic metrics reflect the NAS instead of the container bridge.
- Data volumes, system paths, and device paths are mounted read-only where possible.
- SATA and NVMe health prefer `smartctl`.
- Unavailable fields return `null`, `unknown`, or empty arrays rather than failing the whole API.
