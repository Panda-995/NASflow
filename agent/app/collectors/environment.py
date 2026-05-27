from __future__ import annotations

from pathlib import Path
from typing import Any

from ..settings import settings
from ..utils import list_dir, nsenter_glob, read_text, run_command


def _fans() -> list[dict[str, Any]]:
    fans: list[dict[str, Any]] = []
    for hwmon in list_dir(Path(settings.host_sys) / "class/hwmon"):
        chip_name = read_text(hwmon / "name", hwmon.name)
        hwmon_str = str(hwmon)
        fan_paths = nsenter_glob(hwmon_str, "fan*_input")
        for idx, fan_path_str in enumerate(fan_paths, start=1):
            raw = read_text(fan_path_str)
            try:
                rpm = int(raw)
            except ValueError:
                continue
            fans.append(
                {
                    "id": Path(fan_path_str).stem,
                    "name": f"{chip_name} Fan {idx}",
                    "speed_rpm": rpm,
                    "health": "ok" if rpm > 0 else "warning",
                }
            )
    return fans[:8]


def _ups() -> dict[str, Any]:
    if not settings.enable_ups:
        return {
            "present": False,
            "status": "unknown",
            "battery_pct": None,
            "load_pct": None,
            "runtime_sec": None,
            "health": "unknown",
        }
    code, stdout, _ = run_command(["upsc", "ups"], timeout=3)
    if code != 0:
        return {
            "present": False,
            "status": "unknown",
            "battery_pct": None,
            "load_pct": None,
            "runtime_sec": None,
            "health": "unknown",
        }
    values: dict[str, str] = {}
    for line in stdout.splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            values[key.strip()] = value.strip()
    return {
        "present": True,
        "status": values.get("ups.status", "unknown").lower(),
        "battery_pct": _int_or_none(values.get("battery.charge")),
        "load_pct": _int_or_none(values.get("ups.load")),
        "runtime_sec": _int_or_none(values.get("battery.runtime")),
        "health": "ok",
    }


def _int_or_none(value: str | None) -> int | None:
    if value is None:
        return None
    try:
        return int(float(value))
    except ValueError:
        return None


def collect() -> dict[str, Any]:
    return {"fans": _fans(), "ups": _ups()}

