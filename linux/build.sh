#!/bin/bash

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/bin"
mkdir -p "$OUT"

# Generate config header
bash "$ROOT/gen_config.sh"
if [ $? -ne 0 ]; then
    echo "Build failed at gen_config."
    exit 1
fi

SRC_FSAL="$ROOT/fsal"
SRC_WELT="$ROOT/src"

INC_FSAL="-I$SRC_FSAL -I$SRC_FSAL/internal_platform -I$SRC_FSAL/internal_core -I$SRC_FSAL/internal_net -I$SRC_FSAL/internal_archive"
INC_WELT="-I$SRC_WELT/core -I$SRC_WELT/tokenizer -I$SRC_WELT/compiler -I$SRC_WELT/runtime -I$SRC_WELT/diag"

WELT_OBJS="src/core/core.c src/tokenizer/lexer.c src/runtime/variable.c src/diag/diag.c src/compiler/interpreter.c"
FSAL_DEPS="fsal/internal_platform/platform_linux.c fsal/internal_archive/zipwrap_ps.c fsal/internal_core/config.c fsal/internal_core/ui.c fsal/internal_net/fsnet.c"

# Check for gcc
if ! command -v gcc &> /dev/null; then
    echo "No supported C compiler found (gcc)."
    exit 1
fi

echo "Building fsal..."
CFLAGS="-O2 -Wall $INC_FSAL $INC_WELT"
gcc $CFLAGS $FSAL_DEPS $WELT_OBJS fsal/fsal.c -o "$OUT/fsal"
if [ $? -ne 0 ]; then
    echo "fsal build failed."
    exit 1
fi

echo "Building installer..."
gcc $CFLAGS $FSAL_DEPS fsal/installer.c -o "$OUT/installer"
if [ $? -ne 0 ]; then
    echo "installer build failed."
    exit 1
fi

echo "Build succeeded."
