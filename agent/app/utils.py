from __future__ import annotations

import hashlib
import json
import os
import socket
import subprocess
from pathlib import Path
from typing import Any

from .settings import settings


def read_text(path: str | Path, default: str = "") -> str:
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return default


def host_path(base: str, relative: str) -> Path:
    return Path(base) / relative.lstrip("/")


def run_command(args: list[str], timeout: float = 3.0) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            args,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", str(exc)
    return completed.returncode, completed.stdout.strip(), completed.stderr.strip()


def run_json(args: list[str], timeout: float = 4.0) -> Any | None:
    code, stdout, _ = run_command(args, timeout=timeout)
    if code != 0 or not stdout:
        return None
    try:
        return json.loads(stdout)
    except json.JSONDecodeError:
        return None


def stable_hash(value: str | None) -> str | None:
    if not value:
        return None
    digest = hashlib.sha256(f"{settings.privacy_salt}:{value}".encode()).hexdigest()
    return f"sha256:{digest[:16]}"


def first_existing(paths: list[Path]) -> Path | None:
    for path in paths:
        if path.exists():
            return path
    return None


def hostname() -> str:
    proc_hostname = read_text(host_path(settings.host_proc, "sys/kernel/hostname"))
    if proc_hostname:
        return proc_hostname
    return socket.gethostname()


def list_dir(path: Path) -> list[Path]:
    try:
        return sorted(path.iterdir(), key=lambda item: item.name)
    except OSError:
        return []


def parse_key_value_file(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in read_text(path).splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key.strip()] = value.strip().strip('"')
    return result


def is_rootless_local() -> bool:
    return os.name == "nt"

