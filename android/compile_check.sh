#!/bin/bash
# Compiles Blitzkrieg 1 translation units for arm64 and reports what passes.
#
# The file list comes from each module's .dsp, which is the authoritative
# answer to what the original build actually compiles. Globbing *.cpp is not:
# StreamIO/ carries StructureSaver.cpp and OpenStorage.cpp, both superseded and
# neither built, and counting them understates the port and sends you fixing
# code the game never links.
#
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

# Echoes the .cpp files a module's project builds, relative to the module.
module_sources() {
  local moddir="$1" module="$2"
  local dsp
  dsp=$(ls "$moddir"/*.dsp 2>/dev/null | head -1)
  if [ -n "$dsp" ]; then
    # SOURCE=.\Name.cpp -- take the basename, keep only .cpp
    LC_ALL=C grep -a "^SOURCE=" "$dsp" \
      | sed 's/^SOURCE=//; s/\r$//; s/.*[\\\/]//' \
      | grep -i '\.cpp$' \
      | sort -u
  else
    ls "$moddir" | grep -i '\.cpp$' | sort -u
  fi
}

mkdir -p "$OUT"
pass=0; fail=0; missing=0; failed=(); absent=()

for module in "$@"; do
  moddir="$SRC/$module"
  [ -d "$moddir" ] || { echo "no such module: $module"; exit 1; }
  while read -r base; do
    [ -n "$base" ] || continue
    f="$moddir/$base"
    if [ ! -e "$f" ]; then
      # the project lists it but the checkout does not carry it
      missing=$((missing+1)); absent+=("$module/$base"); continue
    fi
    name="$module/$base"
    obj="$OUT/$(echo "$name" | tr '/' '_').o"
    if "$CXX" "${FLAGS[@]}" -I"$moddir" -c "$f" -o "$obj" 2>"$obj.log"; then
      pass=$((pass+1))
    else
      fail=$((fail+1)); failed+=("$name")
    fi
  done < <(module_sources "$moddir" "$module")
done

echo "compiled: $pass   failed: $fail   listed but absent: $missing"
if [ ${#failed[@]} -gt 0 ]; then
  echo "--- failing units ---"
  printf '%s\n' "${failed[@]}"
fi
if [ ${#absent[@]} -gt 0 ]; then
  echo "--- listed in the project, not in the checkout ---"
  printf '%s\n' "${absent[@]}"
fi
