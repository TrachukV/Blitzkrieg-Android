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

echo "=== wchar_t ABI ==="
"$ROOT/android/tests/check_wchar_abi.sh" || rc=1

echo
echo "=== DXT codec ==="
"$CXX" -std=c++17 -O2 -I"$COMPAT" \
  "$ROOT/android/tests/dxt_codec_test.cpp" "$COMPAT/bk1_s3tc.cpp" \
  -o "$OUT/dxt_codec_test"
"$OUT/dxt_codec_test" || rc=1

echo
echo "=== wave parser ==="
# Links the shipping unit, not a copy of it.
c++ -std=c++17 -O2 \
  "$ROOT/android/tests/wave_parser_test.cpp" "$COMPAT/bk1_wave.cpp" \
  -o "$OUT/wave_parser_test"
"$OUT/wave_parser_test" || rc=1

echo
echo "=== touch gestures ==="
# Three of these can be injected on a device with `adb shell input` and were
# checked that way; the two-finger ones cannot, because a stock emulator image
# refuses a non-root writer on /dev/input. Driving the recogniser directly is
# the only way to cover them -- and it is the same unit the APK ships.
c++ -std=c++17 -O2 \
  "$ROOT/android/tests/touch_gesture_test.cpp" "$ROOT/android/app/src/main/cpp/bk1_touch_gestures.cpp" \
  -o "$OUT/touch_gesture_test"
"$OUT/touch_gesture_test" || rc=1

exit $rc
