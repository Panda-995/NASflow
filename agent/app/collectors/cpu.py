from __future__ import annotations

from pathlib import Path
from typing import Any

from ..settings import settings
from ..utils import host_path, list_dir, nsenter_glob, read_text

_previous_cpu: tuple[int, int] | None = None


def _cpu_totals() -> tuple[int, int] | None:
    line = read_text(host_path(settings.host_proc, "stat")).splitlines()[0:1]
    if not line:
        return None
    parts = line[0].split()
    if not parts or parts[0] != "cpu":
        return None
    values = [int(part) for part in parts[1:] if part.isdigit()]
    idle = values[3] + (values[4] if len(values) > 4 else 0)
    total = sum(values)
    return total, idle


def _usage_pct() -> float | None:
    global _previous_cpu
    current = _cpu_totals()
    if current is None:
        return None
    if _previous_cpu is None:
        _previous_cpu = current
        return 0.0
    total_delta = current[0] - _previous_cpu[0]
    idle_delta = current[1] - _previous_cpu[1]
    _previous_cpu = current
    if total_delta <= 0:
        return 0.0
    return round(max(0.0, min(100.0, (1.0 - idle_delta / total_delta) * 100.0)), 1)


def _load() -> dict[str, float | None]:
    parts = read_text(host_path(settings.host_proc, "loadavg")).split()
    try:
        return {"one": float(parts[0]), "five": float(parts[1]), "fifteen": float(parts[2])}
    except (ValueError, IndexError):
        return {"one": None, "five": None, "fifteen": None}


def _core_count() -> int:
    cpuinfo = read_text(host_path(settings.host_proc, "cpuinfo"))
    count = sum(1 for line in cpuinfo.splitlines() if line.startswith("processor"))
    return count or 1


def _cpu_temp() -> float | None:
    hwmon_root = str(Path(settings.host_sys) / "class/hwmon")
    candidates: list[str] = []
    for hwmon in list_dir(Path(hwmon_root)):
        hwmon_str = str(hwmon)
        name = read_text(hwmon / "name").lower()
        if name in {"k10temp", "coretemp", "cpu_thermal"}:
            candidates.extend(nsenter_glob(hwmon_str, "temp*_input"))
    thermal_root = str(Path(settings.host_sys) / "class/thermal")
    candidates.extend(nsenter_glob(thermal_root, "thermal_zone*/temp"))
    for path in candidates:
        raw = read_text(path)
        try:
            value = float(raw)
        except ValueError:
            continue
        if value > 1000:
            value = value / 1000.0
        if 0.0 < value < 130.0:
            return round(value, 1)
    return None


def collect() -> dict[str, Any]:
    temp = _cpu_temp()
    health = "ok"
    if temp is not None and temp >= 85:
        health = "critical"
    elif temp is not None and temp >= 75:
        health = "warning"
    return {
        "usage_pct": _usage_pct(),
        "temperature_c": temp,
        "core_count": _core_count(),
        "load": _load(),
        "health": health,
    }

