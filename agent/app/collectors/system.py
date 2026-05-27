from __future__ import annotations

import datetime as dt
import socket
from pathlib import Path
from typing import Any

from ..settings import settings
from ..utils import host_path, hostname, parse_key_value_file, read_text, run_json


def _ip_addresses() -> list[str]:
    data = run_json(["ip", "-j", "addr"], timeout=2)
    ips_v4: list[str] = []
    ips_v6: list[str] = []
    if isinstance(data, list):
        for item in data:
            if item.get("ifname") == "lo":
                continue
            for info in item.get("addr_info", []):
                local = info.get("local")
                if local and not local.startswith("127."):
                    if ":" in local:
                        ips_v6.append(local)
                    else:
                        ips_v4.append(local)
    result = ips_v4 + ips_v6
    if result:
        return result
    try:
        return [socket.gethostbyname(socket.gethostname())]
    except OSError:
        return []


def collect() -> dict[str, Any]:
    os_release = parse_key_value_file(Path(settings.host_etc) / "os-release")
    uptime_raw = read_text(host_path(settings.host_proc, "uptime"), "0 0").split()
    try:
        uptime_sec = int(float(uptime_raw[0]))
    except (ValueError, IndexError):
        uptime_sec = 0
    ips = _ip_addresses()
    return {
        "hostname": hostname(),
        "model": os_release.get("PRETTY_NAME", os_release.get("NAME", "Linux")),
        "os_name": os_release.get("NAME", "Linux"),
        "os_version": os_release.get("ZOS_VERSION", os_release.get("VERSION_ID", "")),
        "ip_addresses": ips,
        "primary_ip": ips[0] if ips else "",
        "uptime_sec": uptime_sec,
        "health": "ok",
        "alerts": [],
        "time": dt.datetime.now(dt.UTC).isoformat().replace("+00:00", "Z"),
    }

