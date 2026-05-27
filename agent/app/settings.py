from __future__ import annotations

import os
from dataclasses import dataclass


def _env_bool(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _env_int(name: str, default: int) -> int:
    value = os.getenv(name)
    if value is None:
        return default
    try:
        return int(value)
    except ValueError:
        return default


@dataclass(frozen=True)
class Settings:
    bind_host: str = os.getenv("NAS_AGENT_BIND_HOST", "0.0.0.0")
    port: int = _env_int("NAS_AGENT_PORT", 8088)
    token: str = os.getenv("NAS_AGENT_TOKEN", "")
    host_proc: str = os.getenv("NAS_AGENT_HOST_PROC", "/proc")
    host_sys: str = os.getenv("NAS_AGENT_HOST_SYS", "/sys")
    host_etc: str = os.getenv("NAS_AGENT_HOST_ETC", "/etc")
    enable_smart: bool = _env_bool("NAS_AGENT_ENABLE_SMART", True)
    enable_nvme: bool = _env_bool("NAS_AGENT_ENABLE_NVME", True)
    enable_docker: bool = _env_bool("NAS_AGENT_ENABLE_DOCKER", True)
    enable_ups: bool = _env_bool("NAS_AGENT_ENABLE_UPS", False)
    smart_interval_sec: int = _env_int("NAS_AGENT_SMART_INTERVAL_SEC", 300)
    nvme_interval_sec: int = _env_int("NAS_AGENT_NVME_INTERVAL_SEC", 60)
    docker_interval_sec: int = _env_int("NAS_AGENT_DOCKER_INTERVAL_SEC", 10)
    storage_interval_sec: int = _env_int("NAS_AGENT_STORAGE_INTERVAL_SEC", 30)
    polling_interval_ms: int = _env_int("NAS_AGENT_POLLING_INTERVAL_MS", 5000)
    privacy_salt: str = os.getenv("NAS_AGENT_PRIVACY_SALT", "nas-monitor-agent")


settings = Settings()

