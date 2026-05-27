# NAS Monitor Agent / NAS 监控 Agent

中文：`agent/` 是部署在极空间 NAS 上的只读 Docker 服务。它把 Linux、SMART、RAID、网络和 Docker 状态整理成一个稳定的 JSON API，供 ESP32-S3 状态屏读取。

English: `agent/` is a read-only Docker service for the ZSpace NAS. It normalizes Linux, SMART, RAID, network, and Docker telemetry into a stable JSON API consumed by the ESP32-S3 display.

## Endpoints / 接口

```text
GET /api/v1/health
GET /api/v1/status
```

中文：完整字段定义见 `../docs/API_CONTRACT.md`。如果配置了 `NAS_AGENT_TOKEN`，ESP 请求需要带 `Authorization: Bearer <token>`。

English: The full field contract is documented in `../docs/API_CONTRACT.md`. If `NAS_AGENT_TOKEN` is configured, ESP requests must include `Authorization: Bearer <token>`.

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
  nas-monitor-agent:
    build: .
    network_mode: host
    environment:
      NAS_AGENT_TOKEN: "change-me"
      NAS_AGENT_PORT: "8088"
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

中文：`privileged: true` 是为了让容器能读取 SMART/NVMe/块设备信息。挂载均使用 `:ro`，HTTP API 不提供任何写操作。

English: `privileged: true` allows SMART/NVMe/block-device reads. Host mounts are `:ro`, and the HTTP API exposes no write operations.

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
| `environment` | 风扇、UPS、传感器状态 | fans, UPS, sensor status |
| `workloads` | Docker 容器状态 | Docker container state |
| `data_protection` | 备份和快照占位适配 | backup and snapshot adapter placeholders |

## Verification / 验证

```powershell
cd agent
python -m compileall app
python -c "from app.main import health, status; print(health()); print(status()['schema_version'])"
```
