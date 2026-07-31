#!/bin/bash
# Builds and runs the compatibility-layer tests on the host. These cover the
# pieces that were written rather than ported, where "it compiles" says nothing
# about whether it is right.
set -eu

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COMPAT="$ROOT/android/app/src/main/cpp/compat"
OUT="${OUT:-$ROOT/build/android/tests}"
CXX="${CXX:-clang++}"

mkdir -p "$OUT"
rc=0

echo "=== DXT codec ==="
"$CXX" -std=c++17 -O2 -I"$COMPAT" \
  "$ROOT/android/tests/dxt_codec_test.cpp" "$COMPAT/bk1_s3tc.cpp" \
  -o "$OUT/dxt_codec_test"
"$OUT/dxt_codec_test" || rc=1

exit $rc
