# Build environment for the IH-K663 firmware. amd64 — the TC32 toolchain is a
# Linux x86_64 binary. Used by build.sh locally and by GitHub Actions.
FROM --platform=linux/amd64 debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        make curl unzip tar bzip2 python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
