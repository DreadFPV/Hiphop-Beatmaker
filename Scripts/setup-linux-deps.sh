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
# existing sources: restrict to amd64
if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then
  $SUDO sed -i '/^Architectures:/d; s/^Types: deb$/Types: deb\nArchitectures: amd64 i386/' /etc/apt/sources.list.d/ubuntu.sources
fi
if [ -f /etc/apt/sources.list ]; then
  $SUDO sed -i -E 's/^deb (http[^ ]*)/deb [arch=amd64,i386] \1/' /etc/apt/sources.list
fi
$SUDO tee /etc/apt/sources.list.d/arm64-ports.list >/dev/null <<SRC
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME} main universe
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-updates main universe
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-security main universe
SRC
$SUDO dpkg --add-architecture arm64
$SUDO apt-get update -q
ARMLIBS=""
for l in $LIBS; do ARMLIBS="$ARMLIBS $l:arm64"; done
$SUDO apt-get install -y -q build-essential cmake ninja-build pkg-config g++-aarch64-linux-gnu qemu-user-static binfmt-support libc6:arm64 libstdc++6:arm64 $ARMLIBS
