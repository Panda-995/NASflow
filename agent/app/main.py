from __future__ import annotations

import datetime as dt

from fastapi import Depends, FastAPI

from .auth import require_token
from .status_builder import build_status

app = FastAPI(title="NAS Monitor Agent", version="0.1.0")


@app.get("/api/v1/health")
def health(_: None = Depends(require_token)) -> dict[str, str]:
    return {
        "status": "ok",
        "schema_version": "1.0",
        "agent_version": "0.1.0",
        "time": dt.datetime.now(dt.UTC).isoformat().replace("+00:00", "Z"),
    }


@app.get("/api/v1/status")
def status(_: None = Depends(require_token)) -> dict:
    return build_status()

