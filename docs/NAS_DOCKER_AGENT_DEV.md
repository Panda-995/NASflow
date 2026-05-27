# NAS Docker Agent Development / NAS Docker 端开发文档

中文：NAS Docker Agent 负责在极空间 NAS 上读取只读系统指标，并把它们转成 ESP32 固件能稳定解析的 JSON。

English: The NAS Docker Agent reads read-only system metrics from the ZSpace NAS and converts them into JSON that the ESP32 firmware can parse reliably.

## Goals / 目标

中文：

- 通过 Docker 部署，降低对极空间系统本体的侵入。采用零挂载架构（nsenter），不挂载任何主机卷。
- 对外只暴露 HTTP 只读接口。
- 采集失败时局部降级，不影响其他指标。
- 输出字段与 `API_CONTRACT.md` 保持一致。
- 默认不启用 token 认证。

English:

- Deploy through Docker to reduce intrusion into the ZSpace host system. Uses a zero-mount architecture (nsenter) — no host volume mounts.
- Expose only read-only HTTP endpoints.
- Degrade per collector when data is unavailable, without breaking other metrics.
- Keep output fields aligned with `API_CONTRACT.md`.
- Token authentication is disabled by default.

## Runtime Layout / 运行结构

```text
FastAPI app
  /api/v1/health
  /api/v1/status

Collectors
  system
  cpu
  memory
  storage
  drives
  nvme
  network
  workloads
  data_protection
```

## Docker Compose / Docker Compose 配置

中文：Agent 采用零挂载架构。仅需 pid:host + privileged:true，通过 nsenter 借道宿主机 namespace 读取所有系统信息。仅挂载 Docker socket。

English: The Agent uses a zero-mount architecture. Only pid:host + privileged:true are needed — all system reads go through nsenter in the host namespace. Only the Docker socket is mounted.

```yaml
services:
  nasflow:
    image: ghcr.io/panda-995/nasflow:latest
    container_name: nasflow
    restart: unless-stopped
    network_mode: host
    pid: host
    privileged: true
    environment:
      NAS_AGENT_BIND_HOST: "0.0.0.0"
      NAS_AGENT_PORT: "8088"
      NAS_AGENT_ENABLE_SMART: "true"
      NAS_AGENT_ENABLE_NVME: "true"
      NAS_AGENT_ENABLE_DOCKER: "true"
      NAS_AGENT_POLLING_INTERVAL_MS: "1000"
      NAS_AGENT_SMART_INTERVAL_SEC: "120"
      NAS_AGENT_NVME_INTERVAL_SEC: "30"
      NAS_AGENT_DOCKER_INTERVAL_SEC: "10"
      NAS_AGENT_STORAGE_INTERVAL_SEC: "15"
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock:ro
```

中文：

- `network_mode: host` 让 Agent 看到真实网卡、IP 和链路速率。
- `pid: host` + `privileged: true` 让 nsenter 能进入宿主机 namespace 读取 `/proc`、`/sys`、`/dev` 等。
- 无需挂载任何 `/proc`、`/sys`、`/dev`、`/data_*` 卷（极空间拒绝此类挂载）。
- 仅挂载 Docker socket 用于容器状态读取。

English:

- `network_mode: host` lets the Agent see real NICs, IPs, and link speed.
- `pid: host` + `privileged: true` enables nsenter to enter the host namespace for `/proc`, `/sys`, `/dev` reads.
- No `/proc`, `/sys`, `/dev`, `/data_*` mounts needed (ZSpace rejects such mounts).
- Only the Docker socket is mounted for container state reading.

## Environment Variables / 环境变量

| Variable | 中文 | English |
| --- | --- | --- |
| `NAS_AGENT_BIND_HOST` | 绑定地址，默认 0.0.0.0 | bind address, default 0.0.0.0 |
| `NAS_AGENT_PORT` | HTTP 端口，默认 8088 | HTTP port, default 8088 |
| `NAS_AGENT_TOKEN` | 可选 Bearer token，默认为空（不启用认证） | optional Bearer token, default empty (auth disabled) |
| `NAS_AGENT_POLLING_INTERVAL_MS` | 建议 ESP 轮询间隔，默认 1000ms | recommended ESP polling interval, default 1000ms |
| `NAS_AGENT_SMART_INTERVAL_SEC` | SMART 缓存时间，默认 120s | SMART cache duration, default 120s |
| `NAS_AGENT_NVME_INTERVAL_SEC` | NVMe 缓存时间，默认 30s | NVMe cache duration, default 30s |
| `NAS_AGENT_DOCKER_INTERVAL_SEC` | Docker 状态缓存时间，默认 10s | Docker state cache duration, default 10s |
| `NAS_AGENT_STORAGE_INTERVAL_SEC` | 存储信息缓存时间，默认 15s | storage info cache duration, default 15s |
| `NAS_AGENT_ENABLE_SMART` | 是否启用 SMART 采集，默认 true | enable SMART collection, default true |
| `NAS_AGENT_ENABLE_NVME` | 是否启用 NVMe 采集，默认 true | enable NVMe collection, default true |
| `NAS_AGENT_ENABLE_DOCKER` | 是否启用 Docker 采集，默认 true | enable Docker collection, default true |

## Collector Details / 采集器说明

| Collector | Data source / 数据源 | Output / 输出 |
| --- | --- | --- |
| `system` | nsenter `/etc/os-release`, `/proc/uptime`, `ip -j addr` | NAS identity, IP, uptime |
| `cpu` | nsenter `/proc/stat`, `/proc/loadavg`, `sensors`/`hwmon` | usage, load, temperature |
| `memory` | nsenter `/proc/meminfo` | memory, cache, swap |
| `storage` | nsenter `df -B1 -T`, `/proc/mdstat`, `mdadm --detail` | pools, volumes, RAID state |
| `drives` | nsenter `lsblk`, `smartctl -j -A -H` | SATA disk SMART and health |
| `nvme` | nsenter `lsblk`, `smartctl -j -A -H`, `hwmon` | NVMe temperature and wear |
| `network` | nsenter `/proc/net/dev`, `/sys/class/net`, `ip -j addr` | speed, traffic, errors, drops |
| `workloads` | Docker socket | container state and health |
| `data_protection` | future ZOS adapter | backup and snapshot placeholders |

## Not Collected / 不再采集

中文：以下功能由于硬件限制或用户需求已被移除：

- **风扇转速**：Z4S 硬件不暴露 RPM 接口。
- **UPS 信息**：NUT 服务被 ZOS masked，无法获取。
- **环境页**：ESP 固件已移除环境页面。

English: The following are no longer collected due to hardware limitations or user requirements:

- **Fan RPM**: Z4S hardware does not expose RPM interfaces.
- **UPS info**: NUT service is masked by ZOS, unavailable.
- **Environment page**: Removed from ESP firmware UI.

## ZSpace Result / 极空间当前结果

中文：当前目标 NAS 已确认能提供主机信息、CPU、内存、存储卷、mdraid、SATA SMART、NVMe SMART、网络流量、Docker 状态和多类温度。备份、快照、虚拟机和 ZOS 原生告警仍需要额外适配。

English: The current target NAS can provide host identity, CPU, memory, storage volumes, mdraid, SATA SMART, NVMe SMART, network traffic, Docker state, and several temperatures. Backup, snapshot, VM, and native ZOS alerts need additional adapters.

## Security / 安全

中文：

- Agent 不提供写接口。
- 零挂载架构：不挂载任何主机卷，仅通过 nsenter 只读访问。
- SMART、Docker socket 和设备读取需要更高权限，但代码只做读取。
- 不在仓库中保存 NAS SSH 密码或 token。
- 默认不启用 token 认证。

English:

- The Agent exposes no write endpoints.
- Zero-mount architecture: no host volume mounts, all reads via nsenter.
- SMART, Docker socket, and device reads need elevated access, but the code only reads data.
- NAS SSH passwords and tokens are not stored in the repository.
- Token authentication is disabled by default.

## Verification / 验证

```powershell
cd agent
python -m compileall app
uvicorn app.main:app --host 0.0.0.0 --port 8088
```

Then / 然后：

```powershell
curl http://127.0.0.1:8088/api/v1/health
curl http://127.0.0.1:8088/api/v1/status
```
