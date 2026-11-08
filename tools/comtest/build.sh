#!/bin/sh
# Builds comtest.exe (32-bit Windows console app) with clang + lld, without
# MinGW or the Windows SDK: the few kernel32 functions it needs are declared
# in comtest.c and linked through an import library made from kernel32.def.
set -e
cd "$(dirname "$0")"
OUT=${1:-build}
mkdir -p "$OUT"

llvm-dlltool -k -m i386 -d kernel32.def -l "$OUT/kernel32.lib"
clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector \
      -mno-stack-arg-probe -c comtest.c -o "$OUT/comtest.obj"
lld-link /nologo /subsystem:console /entry:mainCRTStartup /nodefaultlib /machine:x86 \
         /out:"$OUT/comtest.exe" "$OUT/comtest.obj" "$OUT/kernel32.lib"
echo "Built $OUT/comtest.exe"
