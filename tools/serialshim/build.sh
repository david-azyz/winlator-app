#!/bin/sh
# Builds the x86_64 serial ioctl shim that Winlator preloads into Wine via
# BOX64_LD_PRELOAD, and copies it into the app assets.
set -e
cd "$(dirname "$0")"
OUT=${1:-build}
mkdir -p "$OUT"
clang --target=x86_64-linux-gnu -O2 -fPIC -ffreestanding -fno-builtin -fno-stack-protector \
      -nostdlib -shared -fuse-ld=lld -Wl,-soname,libwinlator_serialshim.so -Wl,--allow-shlib-undefined \
      -o "$OUT/libwinlator_serialshim.so" serialshim.c
mkdir -p ../../app/src/main/assets/serial
cp "$OUT/libwinlator_serialshim.so" ../../app/src/main/assets/serial/libwinlator_serialshim.so
echo "Built $OUT/libwinlator_serialshim.so"
