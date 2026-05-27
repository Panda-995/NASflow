from __future__ import annotations

from typing import Any

from ..cache import TimedCache
from ..settings import settings

_cache: TimedCache[dict[str, Any]] = TimedCache()


def _docker() -> dict[str, Any]:
    if not settings.enable_docker:
        return {"running": 0, "stopped": 0, "unhealthy": 0, "containers": []}
    try:
        import docker
    except ImportError:
        return {"running": 0, "stopped": 0, "unhealthy": 0, "containers": []}
    try:
        client = docker.from_env()
        containers = client.containers.list(all=True)
    except Exception:
        return {"running": 0, "stopped": 0, "unhealthy": 0, "containers": []}
    running = stopped = unhealthy = 0
    items: list[dict[str, Any]] = []
    for container in containers[:24]:
        attrs = container.attrs
        state = attrs.get("State", {})
        status = state.get("Status", container.status)
        health_state = state.get("Health", {}).get("Status")
        if status == "running":
            running += 1
        else:
            stopped += 1
        health = "ok"
        if health_state == "unhealthy":
            health = "critical"
            unhealthy += 1
        elif status != "running":
            health = "warning"
        items.append(
            {
                "name": container.name,
                "state": status,
                "health": health,
                "cpu_pct": None,
                "memory_bytes": None,
            }
        )
    return {"running": running, "stopped": stopped, "unhealthy": unhealthy, "containers": items}


def _build() -> dict[str, Any]:
    return {
        "docker": _docker(),
        "virtual_machines": [],
        "services": [],
    }


def collect() -> dict[str, Any]:
    return _cache.get(settings.docker_interval_sec, _build)

