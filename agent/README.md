# NASflow Agent / NASflow 采集端

中文：`agent/` 是部署在极空间 NAS 上的只读 Docker 服务。它通过 nsenter 零挂载架构把 Linux、SMART、RAID、网络和 Docker 状态整理成一个稳定的 JSON API，供 NASflow 状态屏读取。

English: `agent/` is a read-only Docker service for the ZSpace NAS. It uses a zero-mount nsenter architecture to normalize Linux, SMART, RAID, network, and Docker telemetry into a stable JSON API consumed by the NASflow display.

## Endpoints / 接口

```text
GET /api/v1/health
GET /api/v1/status
```

中文：完整字段定义见 `../docs/API_CONTRACT.md`。默认不启用 token 认证。

English: The full field contract is documented in `../docs/API_CONTRACT.md`. Token authentication is disabled by default.

## Local Run / 本地运行

```powershell
cd agent
python -m venv .venv
. .venv/Scripts/Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8088
```

中文：本地运行适合开发和检查 API 结构，但 SMART、mdraid、Docker socket、真实挂载容量等数据需要在 NAS 上运行 Docker 才完整。

English: Local run is useful for development and API checks. SMART, mdraid, Docker socket, and real mount capacity data are only complete when the service runs on the NAS.

## Docker Run On NAS / NAS 上 Docker 部署

Use `docker-compose.example.yml` as the starting point. / 以 `docker-compose.example.yml` 为起点：

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

- **零挂载架构**：仅挂载 Docker socket。所有 `/proc`、`/sys`、`/dev`、存储卷信息通过 nsenter 借道宿主机 namespace 读取。
- `pid: host` + `privileged: true` 让 nsenter 能进入宿主机 namespace。
- `network_mode: host` 让 Agent 看到真实网卡、IP 和链路速率。
- HTTP API 不提供任何写操作。

English:

- **Zero-mount architecture**: Only the Docker socket is mounted. All `/proc`, `/sys`, `/dev`, and storage volume reads go through nsenter in the host namespace.
- `pid: host` + `privileged: true` enables nsenter to enter the host namespace.
- `network_mode: host` exposes real NICs, IPs, and link speed.
- The HTTP API exposes no write operations.

## Collectors / 采集模块

| Collector | 中文 | English |
| --- | --- | --- |
| `system` | 主机名、系统版本、IP、运行时间 | hostname, OS version, IP, uptime |
| `cpu` | CPU 占用、负载、温度 | CPU usage, load, temperature |
| `memory` | 内存、缓存、Swap | memory, cache, swap |
| `storage` | 存储池、卷、RAID/mdraid 状态 | pools, volumes, RAID/mdraid state |
| `drives` | SATA HDD/SSD SMART、温度、坏道、通电时间 | SATA HDD/SSD SMART, temperature, bad sectors, power-on hours |
| `nvme` | M.2/NVMe 容量、温度、寿命/磨损 | M.2/NVMe capacity, temperature, wear |
| `network` | 上传下载速度、网口速率、丢包和错误包 | upload/download speed, link speed, drops, errors |
| `workloads` | Docker 容器状态 | Docker container state |
| `data_protection` | 备份和快照占位适配 | backup and snapshot adapter placeholders |

## Not Collected / 不再采集

- **风扇转速**：Z4S 硬件不暴露 RPM 接口。
- **UPS 信息**：NUT 服务被 ZOS masked。
- **环境采集器**：已从 Agent 中移除，API 不再输出 `environment` 字段。

## Verification / 验证

```powershell
cd agent
python -m compileall app
python -c "from app.main import health, status; print(health()); print(status()['schema_version'])"
```
