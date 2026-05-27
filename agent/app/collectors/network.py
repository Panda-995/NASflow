from __future__ import annotations

import time
from pathlib import Path
from typing import Any

from ..settings import settings
from ..utils import host_path, read_text, run_command, run_json, stable_hash

_previous: dict[str, tuple[float, int, int]] = {}


def _ip_map() -> dict[str, list[str]]:
    data = run_json(["ip", "-j", "addr"], timeout=2)
    result: dict[str, list[str]] = {}
    if not isinstance(data, list):
        return result
    for iface in data:
        name = iface.get("ifname")
        if not name:
            continue
        result[name] = [
            item.get("local")
            for item in iface.get("addr_info", [])
            if item.get("local") and not str(item.get("local")).startswith("127.")
        ]
    return result


def _is_physical(name: str) -> bool:
    if not name.startswith(("eth", "en", "wl")):
        return False
    device_path = str(Path(settings.host_sys) / "class/net" / name / "device")
    code, _, _ = run_command(["nsenter", "-t", "1", "-m", "test", "-d", device_path], timeout=2)
    if code == 0:
        return True
    return Path(device_path).exists()


def _netdev() -> tuple[list[dict[str, Any]], int, int]:
    now = time.time()
    lines = read_text(host_path(settings.host_proc, "net/dev")).splitlines()[2:]
    ips = _ip_map()
    interfaces: list[dict[str, Any]] = []
    total_rx = 0.0
    total_tx = 0.0
    for line in lines:
        if ":" not in line:
            continue
        name, payload = line.split(":", 1)
        name = name.strip()
        if name == "lo":
            continue
        if not _is_physical(name):
            continue
        values = payload.split()
        if len(values) < 16:
            continue
        rx_bytes = int(values[0])
        rx_errors = int(values[2])
        rx_dropped = int(values[3])
        tx_bytes = int(values[8])
        tx_errors = int(values[10])
        tx_dropped = int(values[11])
        prev = _previous.get(name)
        rx_bps = tx_bps = 0.0
        if prev:
            elapsed = max(0.001, now - prev[0])
            rx_bps = max(0.0, (rx_bytes - prev[1]) * 8.0 / elapsed)
            tx_bps = max(0.0, (tx_bytes - prev[2]) * 8.0 / elapsed)
        _previous[name] = (now, rx_bytes, tx_bytes)
        total_rx += rx_bps
        total_tx += tx_bps
        sys_iface = Path(settings.host_sys) / "class/net" / name
        speed_raw = read_text(sys_iface / "speed")
        operstate = read_text(sys_iface / "operstate", "unknown")
        mac_hash = stable_hash(read_text(sys_iface / "address"))
        try:
            speed = int(speed_raw)
        except ValueError:
            speed = None
        interfaces.append(
            {
                "name": name,
                "status": "up" if operstate == "up" else operstate or "unknown",
                "ip_addresses": ips.get(name, []),
                "mac_hash": mac_hash,
                "link_speed_mbps": speed,
                "rx_bps": round(rx_bps),
                "tx_bps": round(tx_bps),
                "rx_errors": rx_errors,
                "tx_errors": tx_errors,
                "rx_dropped": rx_dropped,
                "tx_dropped": tx_dropped,
            }
        )
    return interfaces, round(total_rx), round(total_tx)


def collect() -> dict[str, Any]:
    interfaces, total_rx, total_tx = _netdev()
    return {
        "total_rx_bps": total_rx,
        "total_tx_bps": total_tx,
        "interfaces": interfaces[:8],
        "health": "ok",
    }

