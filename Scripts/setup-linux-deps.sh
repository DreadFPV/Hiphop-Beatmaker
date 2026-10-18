#!/usr/bin/env bash
# Installs build dependencies for Linux.  Usage: setup-linux-deps.sh [x64|arm64]
# arm64 = cross-compile on an x86_64 Ubuntu host (multiarch + aarch64 toolchain).
set -euo pipefail
ARCH="${1:-x64}"
LIBS="libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev libgl1-mesa-dev"
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"

if [ "$ARCH" = "x64" ]; then
  $SUDO apt-get update -q
  $SUDO apt-get install -y -q build-essential cmake ninja-build pkg-config $LIBS
  exit 0
fi

CODENAME="$(. /etc/os-release && echo "$VERSION_CODENAME")"
# every existing one-line source (also "mirror+file:" lines on GitHub runners) -> amd64 only
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list; do
  [ -f "$f" ] || continue
  case "$f" in *arm64-ports.list) continue;; esac
  $SUDO sed -i -E 's/^deb[[:space:]]+([^[[:space:]])/deb [arch=amd64,i386] \1/; s/^deb[[:space:]]+\[(arch=[^]]*)?([^]]*)\]/deb [arch=amd64,i386 \2]/' "$f"
done
for f in /etc/apt/sources.list.d/*.sources; do
  [ -f "$f" ] || continue
  grep -q '^Architectures:' "$f" || $SUDO sed -i 's/^Types:/Architectures: amd64 i386\nTypes:/' "$f"
done
$SUDO tee /etc/apt/sources.list.d/arm64-ports.list >/dev/null <<SRC
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME} main universe
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-updates main universe
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-security main universe
SRC
$SUDO dpkg --add-architecture arm64
report() { # make apt errors visible as GitHub annotations
  if [ -n "${GITHUB_ACTIONS:-}" ]; then grep -E "^(E|W|Err):" "$1" | head -15 | while read -r l; do echo "::error::$l"; done; fi
}
if ! $SUDO apt-get update -q > /tmp/apt-update.log 2>&1; then report /tmp/apt-update.log; cat /tmp/apt-update.log; exit 100; fi
grep -E "^(W|Err):" /tmp/apt-update.log | head -5 || true
ARMLIBS=""
for l in $LIBS; do ARMLIBS="$ARMLIBS $l:arm64"; done
if ! $SUDO apt-get install -y -q build-essential cmake ninja-build pkg-config g++-aarch64-linux-gnu file \
       libc6:arm64 libstdc++6:arm64 $LIBS $ARMLIBS > /tmp/apt-install.log 2>&1; then
  report /tmp/apt-install.log; tail -40 /tmp/apt-install.log; exit 100
fi

