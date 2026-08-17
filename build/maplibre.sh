#!/bin/bash
# Builds MapLibre Native (mbgl-core) for the given XCSoar target and
# installs its headers and static libraries into the given prefix.
# Invoked automatically by build/libmaplibre.mk when building with
# MAPLIBRE=y and no external MAPLIBRE_PREFIX is given.
#
# Unlike the other third-party libraries (build/python/build/libs.py),
# MapLibre Native cannot be consumed as a release tarball: upstream
# publishes no source bundle that includes its vendored git
# submodules, and GitHub archive tarballs omit submodules.  Therefore
# the pinned revision is fetched with git.
#
# Requirements: git, cmake (ninja is used when available).

set -euo pipefail

PREFIX="${1:?usage: $0 <install-prefix> <target>}"
TARGET="${2:?usage: $0 <install-prefix> <target>}"

# The MapLibre Native revision the XCSoar integration was written and
# type-checked against.  Bump deliberately, not accidentally.
readonly MLN_REVISION="b0388d186d582a8535aa3c03e3cc2ef98cb70dc0"

readonly SRC_DIR="$(pwd)/output/src/maplibre-native"
readonly BUILD_DIR="${SRC_DIR}/build-${TARGET}"

case "${TARGET}" in
    UNIX|UNIX32|UNIX64|OPT|WAYLAND|MACOS|OSX64)
        # host build, no cross toolchain required
        ;;
    *)
        echo "Error: build/maplibre.sh: cross-building MapLibre Native" \
             "for TARGET=${TARGET} is not wired up yet" >&2
        echo "(pass MAPLIBRE_PREFIX=... pointing to a prebuilt installation," \
             "or extend this script with the matching CMake toolchain)" >&2
        exit 1
        ;;
esac

# Homebrew is not in the PATH inside the Xcode build sandbox
for dir in /opt/homebrew/bin /usr/local/bin; do
    [ -d "$dir" ] && PATH="$dir:$PATH"
done
export PATH

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake not found; install it e.g. with 'brew install cmake'" >&2
    exit 1
fi

echo "maplibre.sh: building MapLibre Native ${MLN_REVISION} for ${TARGET}"

# 1. Fetch the pinned source revision (shallow: only the pinned
# commit and depth-1 submodules; a full clone would download several
# gigabytes of history)
if [ ! -d "${SRC_DIR}/.git" ]; then
    mkdir -p "${SRC_DIR}"
    git -C "${SRC_DIR}" init -q
    git -C "${SRC_DIR}" remote add origin https://github.com/maplibre/maplibre-native
fi
if [ "$(git -C "${SRC_DIR}" rev-parse HEAD 2>/dev/null)" != "${MLN_REVISION}" ]; then
    git -C "${SRC_DIR}" fetch --depth 1 origin "${MLN_REVISION}"
    git -C "${SRC_DIR}" checkout -q "${MLN_REVISION}"
fi
git -C "${SRC_DIR}" submodule update --init --recursive --depth 1

if [ "$(uname)" = "Darwin" ]; then
    # Use the macOS SDK explicitly; inside the Xcode build sandbox
    # SDKROOT may be unset or point to another platform's SDK, which
    # breaks CMake's compiler checks.
    SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
    export SDKROOT

    # MapLibre's darwin platform enables Swift although no Swift
    # sources are compiled; CMake's Swift check is broken with some
    # Xcode/CMake combinations, so drop Swift from enable_language().
    sed -i '' 's/enable_language(OBJC OBJCXX Swift)/enable_language(OBJC OBJCXX)/' \
        "${SRC_DIR}/platform/darwin/darwin.cmake"
fi

# 2. Configure and build the core library with the OpenGL renderer
# (XCSoar renders through OpenGL ES; on macOS via ANGLE).
GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS=(-G Ninja)
fi

cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" \
    ${GENERATOR_ARGS[@]+"${GENERATOR_ARGS[@]}"} \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DMLN_WITH_OPENGL=ON \
    -DMLN_WITH_METAL=OFF \
    -DMLN_WITH_GLFW=OFF \
    -DMLN_WITH_WERROR=OFF \
    -DMLN_WITH_RTTI=ON

NUM_CPUS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
cmake --build "${BUILD_DIR}" --target mbgl-core -j "${NUM_CPUS}"

# 3. Install headers: the mbgl public headers plus the header-only
# mapbox base libraries they include.
rm -rf "${PREFIX}"
mkdir -p "${PREFIX}/include" "${PREFIX}/lib"

cp -R "${SRC_DIR}/include/mbgl" "${PREFIX}/include/"

for dir in \
    "${SRC_DIR}/vendor/maplibre-native-base/include" \
    "${SRC_DIR}/vendor/maplibre-native-base/deps/geometry.hpp/include" \
    "${SRC_DIR}/vendor/maplibre-native-base/deps/variant/include" \
    "${SRC_DIR}/vendor/maplibre-native-base/deps/geojson.hpp/include"; do
    cp -R "${dir}/." "${PREFIX}/include/"
done

# 4. Install every static library the build produced (mbgl-core plus
# its vendored dependencies); build/libmaplibre.mk links them all.
find "${BUILD_DIR}" -name '*.a' -exec cp {} "${PREFIX}/lib/" \;

if [ ! -f "${PREFIX}/lib/libmbgl-core.a" ]; then
    echo "Error: libmbgl-core.a was not produced" >&2
    exit 1
fi

echo "maplibre.sh: installed $(ls "${PREFIX}/lib" | wc -l | tr -d ' ') libraries to ${PREFIX}"
