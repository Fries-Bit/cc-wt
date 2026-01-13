# Remember to run:
#   chmod +x build.sh
# before running this script.

# also made with AI

#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/bin"

mkdir -p "$OUT"

# Generate config header from r_config.fsal
if ! ./gen_config.sh; then
  echo "Config generation failed."
  exit 1
fi

# Paths to folders
SRC_FSAL="$ROOT/fsal"
SRC_WELT="$ROOT/src"

# Includes
INC_FSAL=(
  -I"$SRC_FSAL"
  -I"$SRC_FSAL/internal_platform"
  -I"$SRC_FSAL/internal_core"
  -I"$SRC_FSAL/internal_net"
  -I"$SRC_FSAL/internal_archive"
)

INC_WELT=(
  -I"$SRC_WELT/core"
  -I"$SRC_WELT/tokenizer"
  -I"$SRC_WELT/compiler"
  -I"$SRC_WELT/runtime"
  -I"$SRC_WELT/diag"
)

# Source files
WELT_OBJS=(
  src/core/core.c
  src/tokenizer/lexer.c
  src/runtime/variable.c
  src/diag/diag.c
  src/compiler/interpreter.c
)

FSAL_DEPS=(
  fsal/internal_platform/platform_linux.c
  fsal/internal_archive/zipwrap_ps.c
  fsal/internal_core/config.c
  fsal/internal_core/ui.c
  fsal/internal_net/fsnet.c
)

# Pick compiler
if command -v gcc >/dev/null 2>&1; then
  CC=gcc
elif command -v clang >/dev/null 2>&1; then
  CC=clang
else
  echo "No supported C compiler found (gcc or clang)."
  exit 1
fi

echo "Building with $CC..."

CFLAGS=(
  -O2
  -Wall
  -D_CRT_SECURE_NO_WARNINGS
  "${INC_FSAL[@]}"
  "${INC_WELT[@]}"
)

LDFLAGS=(
  -lm
  -ldl
  -lpthread
)

# Build
if ! "$CC" \
  "${CFLAGS[@]}" \
  "${FSAL_DEPS[@]}" \
  "${WELT_OBJS[@]}" \
  fsal/fsal.c \
  -o "$OUT/fsal" \
  "${LDFLAGS[@]}"
then
  echo "Build failed."
  exit 1
fi

echo "Build succeeded."
exit 0
