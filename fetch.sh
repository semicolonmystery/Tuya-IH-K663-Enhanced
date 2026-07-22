#!/usr/bin/env bash
# Download the latest prebuilt firmware from GitHub Releases into build/bin/,
# so you can flash without building. This is the recommended path on a
# Raspberry Pi (the build toolchain is x86_64 and does not run natively on ARM).
#
#   ./fetch.sh            # latest release
#   ./fetch.sh v0.1       # a specific tag
#
# Works whether the repo is public or private:
#   - public repo:  needs only `curl`
#   - private repo: needs the GitHub CLI `gh`, logged in (`gh auth login`)
#
# NOTE: if the repo is PRIVATE, Zigbee2MQTT also can't fetch OTA from the release
# URL. For a firmware/OTA project, making the repo public is the simple choice.
set -euo pipefail
cd "$(dirname "$0")"

REPO="semicolonmystery/Tuya-IH-K663-Enhanced"
TAG="${1:-}"
mkdir -p build/bin
ASSETS="ih-k663.bin ih-k663.ota"

# Preferred: gh (authenticated — works for private repos too).
if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
    [[ -z "$TAG" ]] && TAG="$(gh release view --repo "$REPO" --json tagName -q .tagName)"
    echo ">> release: $TAG (via gh)"
    for f in $ASSETS; do
        echo ">> downloading $f"
        gh release download "$TAG" --repo "$REPO" -p "$f" -O "build/bin/$f" --clobber
    done
    echo ">> saved to build/bin/ — flash with:  ./flash.sh app -r"
    exit 0
fi

# Fallback: anonymous curl (public repos only).
command -v curl >/dev/null 2>&1 || { echo "!! need curl (public repo) or gh (private repo)"; exit 1; }
if [[ -z "$TAG" ]]; then
    JSON="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest")" || {
        echo "!! cannot reach releases anonymously."
        echo "   The repo is probably PRIVATE. Either:"
        echo "     - make it public (recommended; also lets Z2M fetch OTA), or"
        echo "     - install the GitHub CLI and log in:  sudo apt install gh && gh auth login"
        exit 1
    }
    TAG="$(printf '%s' "$JSON" | grep -m1 '"tag_name"' | sed -E 's/.*"tag_name": *"([^"]+)".*/\1/')"
fi
echo ">> release: $TAG (public)"
for f in $ASSETS; do
    echo ">> downloading $f"
    curl -fSL "https://github.com/$REPO/releases/download/$TAG/$f" -o "build/bin/$f" \
        || { echo "!! failed to download $f (repo private? see message above)"; exit 1; }
done
echo ">> saved to build/bin/ — flash with:  ./flash.sh app -r"
