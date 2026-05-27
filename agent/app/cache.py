from __future__ import annotations

import time
from dataclasses import dataclass
from threading import Lock
from typing import Callable, Generic, TypeVar

T = TypeVar("T")


@dataclass
class CacheEntry(Generic[T]):
    value: T | None = None
    expires_at: float = 0.0


class TimedCache(Generic[T]):
    def __init__(self) -> None:
        self._entry: CacheEntry[T] = CacheEntry()
        self._lock = Lock()

    def get(self, ttl_sec: int, builder: Callable[[], T]) -> T:
        now = time.time()
        with self._lock:
            if self._entry.value is not None and self._entry.expires_at > now:
                return self._entry.value
            value = builder()
            self._entry = CacheEntry(value=value, expires_at=now + ttl_sec)
            return value

