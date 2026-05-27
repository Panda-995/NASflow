from __future__ import annotations

from pathlib import Path
from typing import Any

from ..cache import TimedCache
from ..settings import settings
from ..utils import list_dir, nsenter_readlink, read_text, run_json, stable_hash

_cache: TimedCache[list[dict[str, Any]]] = TimedCache()


def _lsblk() -> list[dict[str, Any]]:
    data = run_json(
        ["lsblk", "-J", "-b", "-o", "NAME,TYPE,SIZE,MODEL,SERIAL,TRAN,MOUNTPOINTS"],
        timeout=4,
    )
    if not isinstance(data, dict):
        return []
    return data.get("blockdevices", [])


def _smartctl(device: str) -> dict[str, Any] | None:
    return run_json(["smartctl", "-j", "-A", "-H", f"/dev/{device}"], timeout=8)


def _hwmon_temps() -> dict[str, float]:
    temps: dict[str, float] = {}
    for hwmon in list_dir(Path(settings.host_sys) / "class/hwmon"):
        name = read_text(hwmon / "name").lower()
        if name != "nvme":
            continue
        hwmon_str = str(hwmon)
        target = nsenter_readlink(hwmon_str + "/device")
        key = Path(target).name if target else hwmon.name
        raw = read_text(hwmon_str + "/temp1_input")
        try:
            value = float(raw)
        except ValueError:
            continue
        if value > 1000:
            value /= 1000.0
        temps[key] = round(value, 1)
    return temps


def _smart_temp(smart: dict[str, Any] | None) -> float | None:
    if not smart:
        return None
    temp = smart.get("temperature", {})
    if temp.get("current") is not None:
        return float(temp["current"])
    nvme = smart.get("nvme_smart_health_information_log", {})
    if nvme.get("temperature") is not None:
        value = float(nvme["temperature"])
        return round(value - 273.15 if value > 200 else value, 1)
    return None


def _percentage_used(smart: dict[str, Any] | None) -> int | None:
    if not smart:
        return None
    nvme = smart.get("nvme_smart_health_information_log", {})
    value = nvme.get("percentage_used")
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _spare(smart: dict[str, Any] | None) -> int | None:
    if not smart:
        return None
    nvme = smart.get("nvme_smart_health_information_log", {})
    value = nvme.get("available_spare")
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _build() -> list[dict[str, Any]]:
    hwmon_temps = _hwmon_temps()
    devices: list[dict[str, Any]] = []
    slot = 1
    for device in _lsblk():
        if device.get("type") != "disk":
            continue
        name = device.get("name", "")
        if not name.startswith("nvme"):
            continue
        smart = _smartctl(name) if settings.enable_nvme else None
        pct_used = _percentage_used(smart)
        temp = _smart_temp(smart)
        if temp is None:
            temp = hwmon_temps.get(name) or hwmon_temps.get(name.replace("n1", ""))
        health = "ok"
        if pct_used is not None and pct_used >= 90:
            health = "critical"
        elif pct_used is not None and pct_used >= 70:
            health = "warning"
        devices.append(
            {
                "id": name,
                "slot": f"M2_{slot}",
                "model": (device.get("model") or "").strip(),
                "serial_hash": stable_hash(device.get("serial")),
                "capacity_bytes": int(device.get("size") or 0),
                "used_bytes": None,
                "temperature_c": temp,
                "available_spare_pct": _spare(smart),
                "percentage_used_pct": pct_used,
                "wear_pct": pct_used,
                "cache_state": "unknown",
                "health": health,
            }
        )
        slot += 1
    return devices[:8]


def collect() -> list[dict[str, Any]]:
    return _cache.get(settings.nvme_interval_sec, _build)

