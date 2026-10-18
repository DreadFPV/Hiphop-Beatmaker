#!/bin/sh
# Used as CMAKE_CROSSCOMPILING_EMULATOR for the arm64 cross build:
# JUCE's metadata helpers (LV2 .ttl, VST3 moduleinfo.json) are NOT run for the arm64 binary.
# The architecture-independent files are taken from a native x64 build instead (see build.yml).
echo "skip (cross build): $*"
exit 0
