FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        iproute2 \
        util-linux \
        smartmontools \
        mdadm \
        btrfs-progs \
        nvme-cli \
        nut-client \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

EXPOSE 8088
CMD ["sh", "-c", "uvicorn app.main:app --host ${NAS_AGENT_BIND_HOST:-0.0.0.0} --port ${NAS_AGENT_PORT:-8088}"]
