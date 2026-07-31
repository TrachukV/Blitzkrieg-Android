#!/bin/bash
# Builds an installable APK without Gradle.
#
# Gradle is the normal way in and the project is set up for it. This exists
# because it does not need a network, a plugin download or a wrapper jar --
# only the SDK and the NDK -- so the port can be built and put on a device on a
# machine that has nothing else. The two paths produce the same thing.
#
# Usage: ./build_apk.sh [Debug|Release]
set -e

CONFIG="${1:-Release}"
HERE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Library/Android/sdk}}"

pick_newest() { ls "$1" 2>/dev/null | sort -V | tail -1; }

NDK_VERSION="$( pick_newest "$SDK/ndk" )"
BUILD_TOOLS="$( pick_newest "$SDK/build-tools" )"
CMAKE_VERSION="$( pick_newest "$SDK/cmake" )"
PLATFORM="$( ls "$SDK/platforms" 2>/dev/null | grep -E '^android-[0-9]+$' | sort -V | tail -1 )"

for pair in "ndk:$NDK_VERSION" "build-tools:$BUILD_TOOLS" "cmake:$CMAKE_VERSION" "platforms:$PLATFORM"; do
    if [ -z "${pair#*:}" ]; then
        echo "missing $SDK/${pair%%:*} -- install it from the SDK manager" >&2
        exit 1
    fi
done

NDK="$SDK/ndk/$NDK_VERSION"
BT="$SDK/build-tools/$BUILD_TOOLS"
CMAKE="$SDK/cmake/$CMAKE_VERSION/bin/cmake"
BUILD="$HERE/build/$CONFIG"

echo "ndk $NDK_VERSION, build-tools $BUILD_TOOLS, $PLATFORM"

# --- the native library -----------------------------------------------------
# The host make is not assumed either; the NDK carries its own.
MAKE="$NDK/prebuilt/$( uname -s | tr '[:upper:]' '[:lower:]' )-x86_64/bin/make"
[ -x "$MAKE" ] || MAKE="$( command -v make )"

"$CMAKE" -S "$HERE/app/src/main/cpp" -B "$BUILD/native" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DANDROID_STL=c++_static \
    -DANDROID_NDK="$NDK" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_MAKE_PROGRAM="$MAKE" \
    -G "Unix Makefiles" > /dev/null

"$MAKE" -C "$BUILD/native" -j"$( getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4 )"

# The reason -fshort-wchar is safe to use at all: no wide-string symbol may be
# resolved against a libc++ that was built with a 32-bit wchar_t. Checked on
# the library that actually ships, not on a test binary.
NM="$NDK/toolchains/llvm/prebuilt/$( uname -s | tr '[:upper:]' '[:lower:]' )-x86_64/bin/llvm-nm"
if [ -x "$NM" ]; then
    LEAKED="$( "$NM" -u "$BUILD/native/libblitzkrieg.so" 2>/dev/null |
               LC_ALL=C grep -c 'basic_string.*wchar\|IwNS_11char_traitsIwE' || true )"
    if [ "$LEAKED" != "0" ]; then
        echo "wide-string symbols imported from libc++ ($LEAKED) -- see tests/check_wchar_abi.sh" >&2
        exit 1
    fi
    echo "wchar ABI: clean"
fi

# --- the package ------------------------------------------------------------
mkdir -p "$BUILD/apk/lib/arm64-v8a"

# The checked-in manifest carries no package attribute, because the Android
# Gradle plugin sets it from `namespace` and rejects it being in both places.
# aapt2, called directly, still wants it -- so it is put into a copy.
PACKAGE="$( grep -o "namespace *'[^']*'" "$HERE/app/build.gradle" | head -1 | cut -d"'" -f2 )"
sed "s|<manifest |<manifest package=\"$PACKAGE\" |" \
    "$HERE/app/src/main/AndroidManifest.xml" > "$BUILD/AndroidManifest.xml"

"$BT/aapt2" link -o "$BUILD/unsigned.apk" \
    -I "$SDK/platforms/$PLATFORM/android.jar" \
    --manifest "$BUILD/AndroidManifest.xml" \
    --min-sdk-version 24 --target-sdk-version 34 \
    --version-code 1 --version-name 0.1-port

cp "$BUILD/native/libblitzkrieg.so" "$BUILD/apk/lib/arm64-v8a/"
( cd "$BUILD/apk" && zip -q -X "$BUILD/unsigned.apk" lib/arm64-v8a/libblitzkrieg.so )

# --- signing ----------------------------------------------------------------
# A local key, so the result installs. Anything published would be signed with
# a real one; this is here so that building produces something you can put on a
# device immediately.
KEYSTORE="$BUILD/debug.keystore"
if [ ! -f "$KEYSTORE" ]; then
    keytool -genkeypair -keystore "$KEYSTORE" -storepass android -keypass android \
        -alias blitzkrieg -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Blitzkrieg Android Port" 2>/dev/null
fi

"$BT/zipalign" -f -p 4 "$BUILD/unsigned.apk" "$BUILD/aligned.apk"
"$BT/apksigner" sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
    --min-sdk-version 24 --out "$BUILD/Blitzkrieg.apk" "$BUILD/aligned.apk"
"$BT/apksigner" verify "$BUILD/Blitzkrieg.apk"

echo
echo "$BUILD/Blitzkrieg.apk"
echo "install with: adb install -r \"$BUILD/Blitzkrieg.apk\""
