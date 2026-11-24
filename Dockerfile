# ---- Gym Management System: web dashboard ----
FROM python:3.12-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    DATA_DIR=/data \
    PORT=8080 \
    TZ=Asia/Karachi

# tzdata so TZ is honoured (attendance dates use the local day)
RUN apt-get update \
 && apt-get install -y --no-install-recommends tzdata \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --uid 1000 --create-home app \
 && mkdir /data && chown 1000:1000 /data

WORKDIR /srv
COPY app/ /srv/app/

USER 1000
EXPOSE 8080
VOLUME ["/data"]

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD python -c "import os,urllib.request;urllib.request.urlopen('http://127.0.0.1:%s/api/health' % os.environ.get('PORT','8080'),timeout=2)"

CMD ["python", "app/server.py"]
