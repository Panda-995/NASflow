from __future__ import annotations

from typing import Any

from ..settings import settings
from ..utils import host_path, read_text


def _meminfo() -> dict[str, int]:
    values: dict[str, int] = {}
    for line in read_text(host_path(settings.host_proc, "meminfo")).splitlines():
        if ":" not in line:
            continue
        key, rest = line.split(":", 1)
        number = rest.strip().split()[0]
        try:
            values[key] = int(number) * 1024
        except ValueError:
            continue
    return values


def _pct(used: int, total: int) -> float | None:
    if total <= 0:
        return None
    return round(used / total * 100.0, 1)


def collect() -> dict[str, Any]:
    mem = _meminfo()
    total = mem.get("MemTotal", 0)
    available = mem.get("MemAvailable", 0)
    used = max(0, total - available)
    swap_total = mem.get("SwapTotal", 0)
    swap_free = mem.get("SwapFree", 0)
    swap_used = max(0, swap_total - swap_free)
    return {
        "total_bytes": total,
        "used_bytes": used,
        "used_pct": _pct(used, total),
        "available_bytes": available,
        "cache_bytes": mem.get("Cached", 0),
        "swap_total_bytes": swap_total,
        "swap_used_bytes": swap_used,
        "swap_used_pct": _pct(swap_used, swap_total),
        "health": "ok" if _pct(used, total) is None or _pct(used, total) < 85 else "warning",
    }

