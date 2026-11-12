#!/bin/bash
# Cross-build for the Brick (aarch64, glibc 2.40, device SDL2 2.30) with zig cc.
set -e
cd "$(dirname "$0")"
# On the Brick zig lives on the card; on the PC it comes from winget.
if [ -z "$ZIG" ] && [ -x /userdata/claude-tools/zig/zig ]; then
  ZIG=/userdata/claude-tools/zig/zig
  export ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache ZIG_LOCAL_CACHE_DIR=/tmp/zig-cache  # keep cache churn off the FUSE card
fi
ZIG="${ZIG:-$(command -v zig || powershell -c '(Get-Command zig).Source' | tr -d '\r')}"
mkdir -p build
"$ZIG" cc -target aarch64-linux-gnu.2.38 -O2 -mcpu=cortex_a53 -std=gnu11 -Wall -Wno-unused-function \
  -Ithird_party/inc -o build/brickpad src/main.c src/engine.c src/sample.c \
  third_party/lib/libSDL2-2.0.so.0 -lm -lpthread
echo built build/brickpad
