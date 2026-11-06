FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    zip \
    curl \
    ca-certificates \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

ARG UID=1000
ARG GID=1000
RUN groupadd -g ${GID} builder && \
    useradd -u ${UID} -g builder -m -s /bin/bash builder

RUN pip install --no-cache-dir platformio esptool

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

USER builder
ENV HOME=/home/builder
WORKDIR /workspace

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["help"]
