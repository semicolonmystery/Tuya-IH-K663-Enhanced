#!/usr/bin/env bash
# Download the latest prebuilt firmware from GitHub Releases into build/bin/,
# so you can flash without building. This is the recommended path on a
# Raspberry Pi (the build toolchain is x86_64 and does not run natively on ARM).
#
#   ./fetch.sh            # latest release
#   ./fetch.sh v0.1       # a specific tag
#
# Requires: curl.
set -euo pipefail
cd "$(dirname "$0")"

REPO="semicolonmystery/Tuya-IH-K663-Enhanced"
TAG="${1:-}"
mkdir -p build/bin

command -v curl >/dev/null 2>&1 || { echo "!! curl not found: sudo apt install -y curl"; exit 1; }

if [[ -z "$TAG" ]]; then
    echo ">> finding latest release..."
    JSON="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest")" \
        || { echo "!! could not query releases (none published yet?)"; exit 1; }
    TAG="$(printf '%s' "$JSON" | grep -m1 '"tag_name"' | sed -E 's/.*"tag_name": *"([^"]+)".*/\1/')"
    [[ -n "$TAG" ]] || { echo "!! no release found"; exit 1; }
fi

echo ">> release: $TAG"
for f in ih-k663.bin ih-k663.ota; do
    url="https://github.com/$REPO/releases/download/$TAG/$f"
    echo ">> downloading $f"
    curl -fSL "$url" -o "build/bin/$f" || { echo "!! failed to download $f from $url"; exit 1; }
done
echo ">> saved to build/bin/ — flash with:  ./flash.sh app -r"
