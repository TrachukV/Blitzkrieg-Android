#!/bin/bash
# Checks that the -fshort-wchar build imports no basic_string<wchar_t> members.
#
# libc++ ships prebuilt with a 32-bit wchar_t and its mangled names do not
# encode the width, so an imported member would link cleanly and then corrupt
# strings at run time. bk1_wchar_abi.cpp defines them locally instead; this
# proves that it worked.
set -eu

NDK=${NDK:-$HOME/Library/Android/sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COMPAT="$ROOT/android/app/src/main/cpp/compat"
OUT="${OUT:-$ROOT/build/android/tests}"

mkdir -p "$OUT"

# A translation unit that uses std::wstring the way the engine does, compiled
# with the port's own flags, plus the instantiation that is meant to cover it.
cat > "$OUT/wchar_abi_probe.cpp" <<'EOF'
#include <string>
std::wstring Probe()
{
    std::wstring s = L"probe";
    s.append( L"more" );
    s.reserve( 64 );
    s += L'x';
    return s + L"tail";
}
int ProbeSize() { return (int)Probe().size(); }
EOF

"$NDK/aarch64-linux-android24-clang++" -std=c++17 -fshort-wchar -fvisibility=hidden \
  -c "$OUT/wchar_abi_probe.cpp" -o "$OUT/wchar_abi_probe.o"
"$NDK/aarch64-linux-android24-clang++" -std=c++17 -fshort-wchar -fvisibility=hidden \
  -c "$COMPAT/bk1_wchar_abi.cpp" -o "$OUT/wchar_abi_inst.o"

"$NDK/llvm-ar" rcs "$OUT/libwchar_abi.a" "$OUT/wchar_abi_inst.o"
"$NDK/llvm-ld" --version >/dev/null 2>&1 || true

# Link the probe against the instantiation and see what is left undefined.
"$NDK/aarch64-linux-android24-clang++" -fshort-wchar -r \
  "$OUT/wchar_abi_probe.o" "$OUT/wchar_abi_inst.o" -o "$OUT/wchar_abi_linked.o"

imported=$("$NDK/llvm-nm" -u "$OUT/wchar_abi_linked.o" | grep -c "basic_stringIw" || true)
defined=$("$NDK/llvm-nm" --defined-only "$OUT/wchar_abi_linked.o" | grep -c "basic_stringIw" || true)

echo "basic_string<wchar_t> members defined locally: $defined"
echo "basic_string<wchar_t> members imported:        $imported"

if [ "$imported" -ne 0 ]; then
  echo "FAIL: the build would bind these to libc++'s 32-bit-wchar_t versions."
  echo "      Check that bk1_wchar_abi.cpp is in the build, that libc++ is"
  echo "      linked statically, and that -fvisibility=hidden is set."
  exit 1
fi
if [ "$defined" -eq 0 ]; then
  echo "FAIL: nothing was instantiated, so this check proves nothing."
  exit 1
fi
echo "ok: no basic_string<wchar_t> member is imported"
