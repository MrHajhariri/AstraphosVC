FROM debian:12

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root/env
ENTRYPOINT ["/bin/bash"]
