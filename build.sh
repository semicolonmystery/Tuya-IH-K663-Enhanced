#!/usr/bin/env bash
# Local Docker build. Builds the same way CI does: downloads the SDK + TC32
# toolchain into telink_tools/ (cached on the host) and runs make in a
# Debian container. No host compiler is used.
#
#   ./build.sh          # make tools + build .bin/.ota
#   ./build.sh clean    # make clean
#   ./build.sh <target> # any make target
set -euo pipefail
cd "$(dirname "$0")"

IMAGE=ih-k663-build
TARGET="${*:-all}"

echo ">> building docker image $IMAGE"
docker build -q -t "$IMAGE" -f Dockerfile . >/dev/null

echo ">> running: make $TARGET"
docker run --rm -v "$PWD":/work -w /work "$IMAGE" \
    bash -c "make tools && make $TARGET"
