#!/bin/bash
# Compiles Blitzkrieg 1 translation units for arm64 and reports what passes.
# Usage: android/compile_check.sh <module> [more modules...]
#        android/compile_check.sh Misc StreamIO
set -u

NDK=${NDK:-$HOME/Library/Android/sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Sources/src"
COMPAT="$ROOT/android/app/src/main/cpp/compat"
OUT=${OUT:-"$ROOT/build/android/objs"}

CXX="$NDK/aarch64-linux-android24-clang++"
FLAGS=(
  -std=c++17
  -fms-extensions              # resolves the 2137 backslash includes, __int64, calling conventions
  -fdelayed-template-parsing   # MSVC-style two-phase lookup in templates
  -Wno-reserved-user-defined-literal
  -D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES   # ptr_fun, auto_ptr and friends
  -ferror-limit=0
  -include "$COMPAT/bk1_msvc_types.h"
  -I"$COMPAT" -I"$SRC"
)

mkdir -p "$OUT"
pass=0; fail=0; failed=()

for module in "$@"; do
  moddir="$SRC/$module"
  [ -d "$moddir" ] || { echo "no such module: $module"; exit 1; }
  for f in "$moddir"/*.cpp; do
    [ -e "$f" ] || continue
    name="$module/$(basename "$f")"
    obj="$OUT/$(echo "$name" | tr '/' '_').o"
    if "$CXX" "${FLAGS[@]}" -I"$moddir" -c "$f" -o "$obj" 2>"$obj.log"; then
      pass=$((pass+1))
    else
      fail=$((fail+1)); failed+=("$name")
    fi
  done
done

echo "compiled: $pass   failed: $fail"
if [ ${#failed[@]} -gt 0 ]; then
  echo "--- failing units ---"
  printf '%s\n' "${failed[@]}"
fi
