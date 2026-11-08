#!/bin/sh
# Builds comtest.exe (console) and comports.exe (GUI), 32-bit Windows apps,
# with clang + lld, without MinGW or the Windows SDK: the Win32 functions
# they use are declared in the .c files and linked through import libraries
# made from the .def files. The results are copied into the app assets.
set -e
cd "$(dirname "$0")"
OUT=${1:-build}
mkdir -p "$OUT"

lib() { llvm-dlltool -k -m i386 -d "$1" -l "$OUT/$2"; }
cc() { clang --target=i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector \
             -mno-stack-arg-probe -c "$1" -o "$2"; }

lib kernel32.def kernel32.lib
lib kernel32_comports.def kernel32_comports.lib
lib user32.def user32.lib
lib gdi32.def gdi32.lib
lib comctl32.def comctl32.lib

cc comtest.c "$OUT/comtest.obj"
lld-link /nologo /subsystem:console /entry:mainCRTStartup /nodefaultlib /machine:x86 \
         /out:"$OUT/comtest.exe" "$OUT/comtest.obj" "$OUT/kernel32.lib"

cc comports.c "$OUT/comports.obj"
lld-link /nologo /subsystem:windows /entry:WinMainCRTStartup /nodefaultlib /machine:x86 \
         /out:"$OUT/comports.exe" "$OUT/comports.obj" "$OUT/kernel32_comports.lib" \
         "$OUT/user32.lib" "$OUT/gdi32.lib" "$OUT/comctl32.lib"

cp "$OUT/comtest.exe" "$OUT/comports.exe" .
mkdir -p ../../app/src/main/assets/serial
cp comports.exe ../../app/src/main/assets/serial/
echo "Built comtest.exe and comports.exe"
