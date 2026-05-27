# API Contract / API 契约

中文：本文定义 NAS Docker Agent 和 ESP32-S3 固件之间唯一稳定的数据契约。Agent 必须按这里输出 JSON，固件按这里解析和显示。

English: This document defines the single stable data contract between the NAS Docker Agent and the ESP32-S3 firmware. The Agent emits this JSON, and the firmware parses and displays it.

## Base / 基础约定

```text
Base URL: http://{NAS_IP}:{PORT}
Default target: http://192.168.101.12:8088
```

Optional auth / 可选鉴权（默认不启用）：

```http
Authorization: Bearer {token}
```

Units / 单位：

| Suffix | 中文 | English |
| --- | --- | --- |
| `_pct` | 百分比，通常 0 到 100 | percentage, usually 0 to 100 |
| `_bytes` | 字节 | bytes |
| `_bps` | bit/s | bit/s |
| `_mbps` | Mbit/s | Mbit/s |
| `_c` | 摄氏度 | Celsius |
| `_sec` | 秒 | seconds |
| `_rpm` | 转每分钟 | revolutions per minute |

Health values / 健康状态：

```text
ok | warning | critical | unknown | offline
```

Missing data / 缺失数据：

中文：无法采集的数值字段返回 `null` 或省略；状态字段返回 `unknown`；采集失败原因放入 `unavailable`。

English: Unavailable numeric fields return `null` or are omitted. Status fields return `unknown`. Collector failure reasons are reported in `unavailable`.

## GET /api/v1/health

中文：用于 ESP 快速判断 Agent 是否在线。

English: Used by ESP to quickly check whether the Agent is online.

```json
{
  "status": "ok",
  "schema_version": "1.0",
  "agent_version": "0.1.0",
  "time": "2026-05-27T12:00:00Z"
}
```

## GET /api/v1/status

中文：固件主轮询接口。建议 1 秒一次，SMART/NVMe 可由 Agent 内部缓存。

English: Main firmware polling endpoint. The recommended interval is 1 second; SMART/NVMe can be cached inside the Agent.

### Top Level / 顶层结构

```json
{
  "schema_version": "1.0",
  "agent": {"name": "nas-monitor-agent", "version": "0.1.0"},
  "collected_at": "2026-05-27T12:00:00Z",
  "polling": {"recommended_interval_ms": 1000, "smart_cache_sec": 120},
  "nas": {},
  "cpu": {},
  "memory": {},
  "storage": {"pools": [], "volumes": []},
  "drives": [],
  "nvme": [],
  "network": {"interfaces": []},
  "workloads": {"docker": {}},
  "data_protection": {"backups": [], "snapshots": []},
  "unavailable": []
}
```

### Field Map / 字段映射

| Object | Required for display / 显示所需字段 |
| --- | --- |
| `nas` | `hostname`, `primary_ip`, `uptime_sec`, `health`, `alert_count` |
| `cpu` | `usage_pct`, `temperature_c`, `core_count`, `load_one`, `load_five`, `load_fifteen`, `health` |
| `memory` | `total_bytes`, `used_bytes`, `available_bytes`, `cache_bytes`, `swap_total_bytes`, `swap_used_bytes`, `used_pct`, `swap_used_pct`, `health` |
| `storage.pools[]` | `id`, `name`, `raid_type`, `raid_status`, `health`, `total_bytes`, `used_bytes`, `free_bytes`, `used_pct` |
| `storage.volumes[]` | `id`, `name`, `pool_id`, `filesystem`, `health`, `total_bytes`, `used_bytes`, `free_bytes`, `used_pct` |
| `drives[]` | `id`, `bay`, `type`, `model`, `smart_status`, `health`, `capacity_bytes`, `temperature_c`, `bad_sector_count`, `power_on_hours` |
| `nvme[]` | `id`, `slot`, `model`, `cache_state`, `health`, `capacity_bytes`, `used_bytes`, `temperature_c`, `available_spare_pct`, `percentage_used_pct`, `wear_pct` |
| `network` | `total_rx_bps`, `total_tx_bps`, `interfaces[]`, `health` |
| `network.interfaces[]` | `name`, `status`, `ip`, `link_speed_mbps`, `rx_bps`, `tx_bps`, `rx_errors`, `tx_errors`, `rx_dropped`, `tx_dropped` |
| `workloads.docker` | `running`, `stopped`, `unhealthy`, `container_count`, `containers[]` |
| `workloads.docker.containers[]` | `name`, `state`, `health` |
| `data_protection` | `backup_count`, `snapshot_count`, `backups[]`, `snapshots[]` |

### Compact Example / 精简示例

```json
{
  "schema_version": "1.0",
  "agent": {"name": "nas-monitor-agent", "version": "0.1.0"},
  "collected_at": "2026-05-27T12:00:00Z",
  "polling": {"recommended_interval_ms": 1000, "smart_cache_sec": 120},
  "nas": {
    "hostname": "Z423-8OU2",
    "primary_ip": "192.168.101.12",
    "uptime_sec": 123456,
    "health": "ok",
    "alert_count": 0
  },
  "cpu": {
    "usage_pct": 23.4,
    "temperature_c": 55.2,
    "core_count": 16,
    "load_one": 0.71,
    "load_five": 0.62,
    "load_fifteen": 0.58,
    "health": "ok"
  },
  "memory": {
    "total_bytes": 34359738368,
    "used_bytes": 21152713932,
    "available_bytes": 13207024436,
    "cache_bytes": 5368709120,
    "swap_total_bytes": 4294967296,
    "swap_used_bytes": 536870912,
    "used_pct": 61.6,
    "swap_used_pct": 12.5,
    "health": "ok"
  },
  "storage": {
    "pool_count": 1,
    "volume_count": 1,
    "pools": [
      {"id": "data_s001", "name": "/data_s001", "raid_type": "mdraid/btrfs", "raid_status": "healthy", "health": "ok", "total_bytes": 4000000000000, "used_bytes": 2000000000000, "free_bytes": 2000000000000, "used_pct": 50.0}
    ],
    "volumes": [
      {"id": "data_s001", "name": "/data_s001", "pool_id": "data_s001", "filesystem": "btrfs", "health": "ok", "total_bytes": 4000000000000, "used_bytes": 2000000000000, "free_bytes": 2000000000000, "used_pct": 50.0}
    ]
  },
  "drives": [
    {"id": "sda", "bay": "1", "type": "hdd", "model": "ST4000VX016", "smart_status": "passed", "health": "ok", "capacity_bytes": 4000000000000, "temperature_c": 38.0, "bad_sector_count": 0, "power_on_hours": 12345}
  ],
  "nvme": [
    {"id": "nvme0n1", "slot": "M2_1", "model": "NVMe", "cache_state": "unknown", "health": "ok", "capacity_bytes": 1000204886016, "used_bytes": 0, "temperature_c": 51.0, "available_spare_pct": 100, "percentage_used_pct": 4, "wear_pct": 4}
  ],
  "network": {
    "total_rx_bps": 12500000,
    "total_tx_bps": 3800000,
    "health": "ok",
    "interface_count": 1,
    "interfaces": [
      {"name": "eth1", "status": "up", "ip": "192.168.101.12", "link_speed_mbps": 2500, "rx_bps": 12000000, "tx_bps": 3600000, "rx_errors": 0, "tx_errors": 0, "rx_dropped": 0, "tx_dropped": 0}
    ]
  },
  "workloads": {"docker": {"running": 4, "stopped": 1, "unhealthy": 1, "container_count": 5, "containers": []}},
  "data_protection": {"backup_count": 0, "snapshot_count": 0, "backups": [], "snapshots": []},
  "unavailable": []
}
```

## UI Page Mapping / 页面字段映射（7 页）

| Page / 页面 | Fields / 字段 |
| --- | --- |
| 总览 | `nas`, `cpu.usage_pct`, `memory.used_pct`, all storage pools total, `network.total_rx_bps`, `network.total_tx_bps`, first drive temp, `workloads.docker` |
| 性能 | `cpu`（环形图 + 负载）, `memory`（含 Swap）, health chips |
| 存储 | `storage.pools[]` 合计 `total_bytes/used_bytes/free_bytes`，并展示最多 3 个单池健康摘要 |
| 硬盘 | `drives[]` + `nvme[]`（HDD/SSD 与 M.2/NVMe 卡片：温度、容量、坏块、通电时间、磨损、缓存状态） |
| 网络 | `network.interfaces[]`（IP + 速率 + 错误/丢包，最多 6 个）, traffic totals |
| 服务 | `workloads.docker`（运行/停止/异常统计 + `containers[]` 容器列表） |
| 后台 | ESP 本地配置状态，不依赖 NAS API；Web 后台通过 `GET /api/config` 暴露脱敏配置，通过 `GET /api/test` 测试 NAS Agent 健康接口 |

## Removed / 已移除

中文：

- **环境页面**：ESP 固件已不再渲染环境页（风扇、UPS）。
- Agent 不再采集 `environment` 字段，`/api/v1/status` 输出中不再包含 `environment` 对象。

English:

- **Environment page**: The ESP firmware no longer renders an environment page (fans, UPS).
- The Agent no longer collects the `environment` field — the `environment` object is no longer present in `/api/v1/status` output.
