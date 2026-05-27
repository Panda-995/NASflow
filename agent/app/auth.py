from __future__ import annotations

from fastapi import Header, HTTPException

from .settings import settings


def require_token(authorization: str | None = Header(default=None)) -> None:
    if not settings.token:
        return
    expected = f"Bearer {settings.token}"
    if authorization != expected:
        raise HTTPException(status_code=401, detail="invalid token")

