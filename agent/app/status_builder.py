from __future__ import annotations

import datetime as dt
from typing import Any, Callable

from .collectors import cpu, data_protection, drives, environment, memory, network, nvme, storage, system, workloads
from .settings import settings


def _safe_collect(field: str, unavailable: list[dict[str, str]], fn: Callable[[], Any], fallback: Any) -> Any:
    try:
        return fn()
    except Exception as exc:
        unavailable.append({"field": field, "reason": exc.__class__.__name__})
        return fallback


def build_status() -> dict[str, Any]:
    unavailable: list[dict[str, str]] = []
    nas = _safe_collect("nas", unavailable, system.collect, {})
    cpu_status = _safe_collect("cpu", unavailable, cpu.collect, {})
    memory_status = _safe_collect("memory", unavailable, memory.collect, {})
    storage_status = _safe_collect("storage", unavailable, storage.collect, {"pools": [], "volumes": []})
    drive_status = _safe_collect("drives", unavailable, drives.collect, [])
    nvme_status = _safe_collect("nvme", unavailable, nvme.collect, [])
    network_status = _safe_collect("network", unavailable, network.collect, {"interfaces": []})
    environment_status = _safe_collect("environment", unavailable, environment.collect, {"fans": [], "ups": {}})
    workload_status = _safe_collect("workloads", unavailable, workloads.collect, {})
    protection_status = _safe_collect("data_protection", unavailable, data_protection.collect, {})
    nas["health"] = _overall_health(
        [
            nas.get("health"),
            cpu_status.get("health"),
            memory_status.get("health"),
            network_status.get("health"),
            *(pool.get("health") for pool in storage_status.get("pools", [])),
            *(drive.get("health") for drive in drive_status),
            *(disk.get("health") for disk in nvme_status),
        ]
    )
    return {
        "schema_version": "1.0",
        "agent": {"name": "nas-monitor-agent", "version": "0.1.0"},
        "collected_at": dt.datetime.now(dt.UTC).isoformat().replace("+00:00", "Z"),
        "polling": {
            "recommended_interval_ms": settings.polling_interval_ms,
            "smart_cache_sec": settings.smart_interval_sec,
        },
        "nas": nas,
        "cpu": cpu_status,
        "memory": memory_status,
        "storage": storage_status,
        "drives": drive_status,
        "nvme": nvme_status,
        "network": network_status,
        "environment": environment_status,
        "workloads": workload_status,
        "data_protection": protection_status,
        "unavailable": unavailable,
    }


def _overall_health(values: list[str | None]) -> str:
    if "critical" in values:
        return "critical"
    if "warning" in values:
        return "warning"
    if any(value in {"ok", "warning", "critical"} for value in values):
        return "ok"
    return "unknown"

