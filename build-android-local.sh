#!/usr/bin/env bash
set -Eeuo pipefail

QT_ANDROID="${QT_ANDROID:-$HOME/Qt/6.8.2/android_arm64_v8a}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/26.1.10909125}"
BUILD_DIR="${BUILD_DIR:-build-android}"

for path in \
  "$QT_ANDROID/bin/qt-cmake" \
  "$ANDROID_SDK_ROOT" \
  "$ANDROID_NDK_ROOT"; do
    if [ ! -e "$path" ]; then
        echo "FEHLT: $path" >&2
        exit 1
    fi
done

rm -rf "$BUILD_DIR"
"$QT_ANDROID/bin/qt-cmake" \
  -S . \
  -B "$BUILD_DIR" \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"

cmake --build "$BUILD_DIR" --target apk --parallel
find "$BUILD_DIR" -type f -name '*.apk' -print
