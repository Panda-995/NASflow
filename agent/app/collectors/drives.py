from __future__ import annotations

from pathlib import Path
from typing import Any

from ..cache import TimedCache
from ..settings import settings
from ..utils import run_json, stable_hash

_cache: TimedCache[list[dict[str, Any]]] = TimedCache()


def _lsblk() -> list[dict[str, Any]]:
    data = run_json(
        [
            "lsblk",
            "-J",
            "-b",
            "-o",
            "NAME,TYPE,SIZE,MODEL,SERIAL,ROTA,TRAN,FSTYPE,MOUNTPOINTS",
        ],
        timeout=4,
    )
    if not isinstance(data, dict):
        return []
    devices = data.get("blockdevices")
    return devices if isinstance(devices, list) else []


def _smart(device: str) -> dict[str, Any] | None:
    return run_json(["smartctl", "-j", "-A", "-H", f"/dev/{device}"], timeout=8)


def _attr(table: list[dict[str, Any]], names: set[str]) -> int:
    for item in table:
        if item.get("name") in names:
            raw = item.get("raw", {})
            try:
                return int(raw.get("value", 0))
            except (TypeError, ValueError):
                return 0
    return 0


def _temperature(smart: dict[str, Any] | None) -> float | None:
    if not smart:
        return None
    temp = smart.get("temperature", {})
    value = temp.get("current")
    if value is not None:
        return float(value)
    table = smart.get("ata_smart_attributes", {}).get("table", [])
    for item in table:
        if item.get("name") in {"Temperature_Celsius", "Airflow_Temperature_Cel"}:
            raw = item.get("raw", {})
            try:
                return float(raw.get("value"))
            except (TypeError, ValueError):
                return None
    return None


def _build() -> list[dict[str, Any]]:
    drives: list[dict[str, Any]] = []
    bay = 1
    for device in _lsblk():
        if device.get("type") != "disk":
            continue
        name = device.get("name", "")
        tran = (device.get("tran") or "").lower()
        rota = bool(device.get("rota"))
        if not name.startswith("sd"):
            continue
        smart = _smart(name) if settings.enable_smart else None
        table = smart.get("ata_smart_attributes", {}).get("table", []) if smart else []
        bad = _attr(table, {"Reallocated_Sector_Ct"}) + _attr(
            table, {"Current_Pending_Sector", "Offline_Uncorrectable", "Runtime_Bad_Block"}
        )
        power_hours = _attr(table, {"Power_On_Hours"})
        passed = smart.get("smart_status", {}).get("passed") if smart else None
        health = "ok" if passed is True and bad == 0 else "warning" if passed is not False else "critical"
        drives.append(
            {
                "id": name,
                "bay": str(bay),
                "type": "hdd" if rota else "ssd",
                "model": (device.get("model") or "").strip(),
                "serial_hash": stable_hash(device.get("serial")),
                "capacity_bytes": int(device.get("size") or 0),
                "temperature_c": _temperature(smart),
                "smart_status": "passed" if passed is True else "failed" if passed is False else "unknown",
                "health": health,
                "bad_sector_count": bad,
                "power_on_hours": power_hours,
                "role": "data",
                "pool_id": None,
            }
        )
        bay += 1
    return drives[:24]


def collect() -> list[dict[str, Any]]:
    return _cache.get(settings.smart_interval_sec, _build)

