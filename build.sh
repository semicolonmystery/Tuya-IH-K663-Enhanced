#!/usr/bin/env bash
# Local Docker build. Builds the same way CI does: downloads the SDK + TC32
# toolchain into telink_tools/ (cached on the host) and runs make in a Debian
# container. No host compiler is used.
#
#   ./build.sh          # make tools + build .bin/.ota
#   ./build.sh clean    # make clean
#   ./build.sh <target> # any make target
#
# Requirements: Docker, and your user in the `docker` group (or run with sudo).
#
# NOTE: the TC32 toolchain is an x86_64 Linux binary, so the build image is
# linux/amd64. On an x86_64 machine this runs natively. On ARM (e.g. a Raspberry
# Pi) it runs under QEMU emulation, which is SLOW — on a Pi you almost certainly
# want ./fetch.sh (download the prebuilt firmware) instead of building.
set -euo pipefail
cd "$(dirname "$0")"

IMAGE=ih-k663-build
TARGET="${*:-all}"

# ---- Docker present? -------------------------------------------------------
if ! command -v docker >/dev/null 2>&1; then
    cat >&2 <<'EOF'
!! docker not found.

On a Raspberry Pi you probably don't want to build at all — the toolchain is
x86_64 and only runs here under slow emulation. Instead download the prebuilt
firmware:

    ./fetch.sh && ./flash.sh app -r

To install Docker anyway:  curl -fsSL https://get.docker.com | sudo sh
EOF
    exit 1
fi

# ---- Docker daemon reachable? ----------------------------------------------
if ! docker info >/dev/null 2>&1; then
    cat >&2 <<'EOF'
!! cannot talk to the Docker daemon (permission or not running).

Fix permissions (then log out/in):   sudo usermod -aG docker "$USER"
Or just run this script with sudo:    sudo ./build.sh
EOF
    exit 1
fi

# ---- ARM host: set up amd64 emulation --------------------------------------
ARCH="$(uname -m)"
if [[ "$ARCH" != "x86_64" && "$ARCH" != "amd64" ]]; then
    echo ">> host is $ARCH; the x86_64 toolchain needs amd64 emulation (slow)."
    echo ">> tip: on a Pi, './fetch.sh' downloads a prebuilt binary in seconds."
    echo ">> installing binfmt/qemu amd64 handler..."
    docker run --privileged --rm tonistiigi/binfmt --install amd64 >/dev/null 2>&1 \
        || echo "!! binfmt install failed; build may not work. Consider ./fetch.sh"
fi

echo ">> building docker image $IMAGE"
docker build -q -t "$IMAGE" -f Dockerfile . >/dev/null

echo ">> running: make $TARGET"
docker run --rm -v "$PWD":/work -w /work "$IMAGE" \
    bash -c "make tools && make $TARGET"
