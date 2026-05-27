# NAS Docker Agent Development / NAS Docker 端开发文档

中文：NAS Docker Agent 负责在极空间 NAS 上读取只读系统指标，并把它们转成 ESP32 固件能稳定解析的 JSON。

English: The NAS Docker Agent reads read-only system metrics from the ZSpace NAS and converts them into JSON that the ESP32 firmware can parse reliably.

## Goals / 目标

中文：

- 通过 Docker 部署，降低对极空间系统本体的侵入。
- 对外只暴露 HTTP 只读接口。
- 采集失败时局部降级，不影响其他指标。
- 输出字段与 `API_CONTRACT.md` 保持一致。

English:

- Deploy through Docker to reduce intrusion into the ZSpace host system.
- Expose only read-only HTTP endpoints.
- Degrade per collector when data is unavailable, without breaking other metrics.
- Keep output fields aligned with `API_CONTRACT.md`.

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
  environment
  workloads
  data_protection
```

## Docker Compose / Docker Compose 配置

中文：建议先复制 `agent/docker-compose.example.yml`，修改 token 后在 NAS 上部署。

English: Start from `agent/docker-compose.example.yml`, change the token, then deploy on the NAS.

```yaml
services:
  nas-monitor-agent:
    build: .
    network_mode: host
    restart: unless-stopped
    environment:
      NAS_AGENT_TOKEN: "change-me"
      NAS_AGENT_PORT: "8088"
      NAS_AGENT_POLLING_INTERVAL_MS: "5000"
      NAS_AGENT_SMART_INTERVAL_SEC: "300"
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /etc:/host/etc:ro
      - /dev:/dev:ro
      - /data_s001:/data_s001:ro
      - /data_s002:/data_s002:ro
      - /data_n003:/data_n003:ro
      - /zspace:/zspace:ro
      - /var/run/docker.sock:/var/run/docker.sock:ro
    privileged: true
```

中文：`network_mode: host` 可以让 Agent 看到真实网卡、IP 和链路速率；`/dev` 只读挂载配合 `privileged` 用于 SMART/NVMe 读取。

English: `network_mode: host` lets the Agent see real NICs, IPs, and link speed. The read-only `/dev` mount plus `privileged` enables SMART/NVMe reads.

## Environment Variables / 环境变量

| Variable | 中文 | English |
| --- | --- | --- |
| `NAS_AGENT_TOKEN` | 可选 Bearer token | optional Bearer token |
| `NAS_AGENT_PORT` | HTTP 端口，默认 8088 | HTTP port, default 8088 |
| `NAS_AGENT_POLLING_INTERVAL_MS` | 建议 ESP 轮询间隔 | recommended ESP polling interval |
| `NAS_AGENT_SMART_INTERVAL_SEC` | SMART/NVMe 建议缓存时间 | recommended SMART/NVMe cache duration |
| `NAS_AGENT_HOST_PROC` | 容器内 host proc 路径 | host proc path inside container |
| `NAS_AGENT_HOST_SYS` | 容器内 host sys 路径 | host sys path inside container |
| `NAS_AGENT_HOST_ETC` | 容器内 host etc 路径 | host etc path inside container |

## Collector Details / 采集器说明

| Collector | Data source / 数据源 | Output / 输出 |
| --- | --- | --- |
| `system` | `/etc/os-release`, `/proc/uptime`, `ip -j addr` | NAS identity, IP, uptime |
| `cpu` | `/proc/stat`, `/proc/loadavg`, `sensors`/`hwmon` | usage, load, temperature |
| `memory` | `/proc/meminfo` | memory, cache, swap |
| `storage` | `df -B1 -T`, `/proc/mdstat`, `mdadm --detail` | pools, volumes, RAID state |
| `drives` | `lsblk`, `smartctl -j -A -H` | SATA disk SMART and health |
| `nvme` | `lsblk`, `smartctl -j -A -H`, `hwmon` | NVMe temperature and wear |
| `network` | `/proc/net/dev`, `/sys/class/net`, `ip -j addr` | speed, traffic, errors, drops |
| `environment` | `hwmon`, `sensors`, `upsc` | fans, UPS, sensor health |
| `workloads` | Docker socket | container state and health |
| `data_protection` | future ZOS adapter | backup and snapshot placeholders |

## ZSpace Result / 极空间当前结果

中文：当前目标 NAS 已确认能提供主机信息、CPU、内存、存储卷、mdraid、SATA SMART、NVMe SMART、网络流量、Docker 状态和多类温度。风扇转速、UPS、备份、快照、虚拟机和 ZOS 原生告警仍需要额外适配。

English: The current target NAS can provide host identity, CPU, memory, storage volumes, mdraid, SATA SMART, NVMe SMART, network traffic, Docker state, and several temperatures. Fan RPM, UPS, backup, snapshot, VM, and native ZOS alerts need additional adapters.

## Security / 安全

中文：

- Agent 不提供写接口。
- Docker 挂载尽量使用 `:ro`。
- SMART、Docker socket 和设备读取需要更高权限，但代码只做读取。
- 不在仓库中保存 NAS SSH 密码或 token。

English:

- The Agent exposes no write endpoints.
- Docker mounts use `:ro` wherever possible.
- SMART, Docker socket, and device reads need elevated access, but the code only reads data.
- NAS SSH passwords and tokens are not stored in the repository.

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
