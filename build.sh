#!/usr/bin/env bash
# Build libStructureLimitRemover.so (arm64-v8a) for LeviLauncher.
#
# Usage:
#   ANDROID_NDK_HOME=/path/to/ndk bash build.sh
#
# Environment variables:
#   ANDROID_NDK_HOME / ANDROID_NDK_ROOT   NDK root (auto-detected from
#                                         ANDROID_SDK_ROOT/ndk if unset)
#   CMAKE                                  cmake executable (default: cmake)
#   NINJA                                  ninja executable (default: ninja)
#   JOBS                                   parallel jobs (default: nproc)
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$SRC/build"

if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
    if [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
        ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
    elif [[ -n "${ANDROID_SDK_ROOT:-}" && -d "$ANDROID_SDK_ROOT/ndk" ]]; then
        ANDROID_NDK_HOME="$(ls -1d "$ANDROID_SDK_ROOT"/ndk/* 2>/dev/null | tail -1)"
    fi
fi
if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
    echo "Cannot locate the Android NDK. Set ANDROID_NDK_HOME." >&2
    exit 1
fi

CMAKE="${CMAKE:-cmake}"
NINJA="${NINJA:-ninja}"
TOOLCHAIN="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
if [[ ! -f "$TOOLCHAIN" ]]; then
    echo "Android toolchain not found: $TOOLCHAIN" >&2
    exit 1
fi

"$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN" \
    "-DCMAKE_MAKE_PROGRAM=$NINJA" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release

"$CMAKE" --build "$BUILD" -j "${JOBS:-$(nproc)}"

echo "Built: $BUILD/libStructureLimitRemover.so"
