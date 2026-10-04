#!/usr/bin/env bash
# ======================================================================
#  Fruity-Prime native C++ local build for macOS (Clang)
#  Mirrors .github/workflows/native-cpp-macos.yml and build-cpp.bat.
#
#  Usage:
#    tools/build/build-cpp-macos.sh [config] [options...]
#
#    config  : Release (default), Debug, RelWithDebInfo, MinSizeRel
#    options : deps       install Homebrew packages and Qt first
#              clean      delete the build tree before configuring
#              configure  run the CMake configure step only
#              vulkan     require the Vulkan backend (MoltenVK); with "deps",
#                         also install MoltenVK and the vcpkg ports it needs
#              run        start the built game afterwards (-launcher)
#
#  Environment overrides:
#    QT_ROOT     Qt kit, e.g. ~/Qt/6.11.2/macos  (default: newest ~/Qt/6.*/{macos,clang_64},
#                then Homebrew's qt)
#    VCPKG_ROOT  vcpkg checkout for VMA and glslc (default: ~/vcpkg)
#    BUILD_JOBS  parallel build jobs             (default: hw.ncpu)
#
#  Output: tools/build/out/macos-<config>/
# ======================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_ROOT="$SCRIPT_DIR/out"

CONFIG=Release
DO_DEPS=0; DO_CLEAN=0; CONFIGURE_ONLY=0; DO_RUN=0; DO_VULKAN=0
BUILD_JOBS="${BUILD_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

usage() {
    sed -n '3,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

for arg in "$@"; do
    case "$(echo "$arg" | tr '[:upper:]' '[:lower:]')" in
        release)        CONFIG=Release ;;
        debug)          CONFIG=Debug ;;
        relwithdebinfo) CONFIG=RelWithDebInfo ;;
        minsizerel)     CONFIG=MinSizeRel ;;
        deps)           DO_DEPS=1 ;;
        clean)          DO_CLEAN=1 ;;
        configure)      CONFIGURE_ONLY=1 ;;
        run)            DO_RUN=1 ;;
        vulkan)         DO_VULKAN=1 ;;
        help|-h|--help) usage; exit 0 ;;
        *) echo "[build] ERROR: unknown argument \"$arg\"" >&2; usage; exit 2 ;;
    esac
done

[[ "$(uname -s)" == Darwin ]] || { echo '[build] ERROR: this script is for macOS' >&2; exit 1; }
[[ -f "$REPO_ROOT/CMakeLists.txt" ]] || { echo "[build] ERROR: CMakeLists.txt not found under $REPO_ROOT" >&2; exit 1; }

# Homebrew on Apple Silicon / Intel
for p in /opt/homebrew/bin /usr/local/bin; do
    [[ -d "$p" ]] && case ":$PATH:" in *":$p:"*) ;; *) PATH="$p:$PATH" ;; esac
done
export PATH

# Vulkan on macOS is MoltenVK behind the loader. VMA and glslc are not in
# Homebrew (and shaderc does not build on a tier-3 one), so they come from
# vcpkg, as on the Linux and Android CI.
VULKAN_BREW_PKGS="molten-vk vulkan-loader vulkan-headers"
VULKAN_VCPKG_PORTS="vulkan-memory-allocator shaderc"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
case "$(uname -m)" in arm64) VCPKG_TRIPLET=arm64-osx ;; *) VCPKG_TRIPLET=x64-osx ;; esac
VCPKG_INSTALLED="$VCPKG_ROOT/installed/$VCPKG_TRIPLET"

BREW_PKGS="cmake ninja ccache autoconf autoconf-archive automake libtool curl libarchive zlib glfw freetype jpeg-turbo openal-soft sevenzip"

find_qt() {
    if [[ -n "${QT_ROOT:-}" ]]; then echo "$QT_ROOT"; return; fi
    local found
    found="$(ls -d "$HOME"/Qt/6.*/macos "$HOME"/Qt/6.*/clang_64 2>/dev/null | sort -V | tail -1 || true)"
    if [[ -z "$found" ]] && command -v brew >/dev/null; then
        # Homebrew's qt formula is a complete Qt 6 kit as well.
        found="$(brew --prefix qt 2>/dev/null || true)"
        [[ -f "$found/lib/cmake/Qt6/Qt6Config.cmake" ]] || found=""
    fi
    echo "$found"
}

if [[ "$DO_DEPS" == 1 ]]; then
    command -v brew >/dev/null || { echo '[build] ERROR: Homebrew is required: https://brew.sh' >&2; exit 1; }
    echo "[build] Installing Homebrew packages..."
    # One at a time and only what is missing: on a tier-3 Homebrew (older
    # macOS, Intel) formulae build from source, and one that fails would
    # otherwise abort every other install in the same command.
    FAILED=""
    ALL_PKGS="$BREW_PKGS"
    [[ "$DO_VULKAN" == 1 ]] && ALL_PKGS="$ALL_PKGS $VULKAN_BREW_PKGS"
    for pkg in $ALL_PKGS; do
        brew list --versions "$pkg" >/dev/null 2>&1 && continue
        echo "[build] brew install $pkg"
        brew install "$pkg" || FAILED="$FAILED $pkg"
    done
    [[ -n "$FAILED" ]] && echo "[build] WARNING: Homebrew could not install:$FAILED (the configure step will say if any is required)" >&2
    if [[ "$DO_VULKAN" == 1 ]]; then
        if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
            echo "[build] Installing vcpkg into $VCPKG_ROOT ..."
            [[ -d "$VCPKG_ROOT/.git" ]] || git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
            "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
        fi
        ports=()
        for port in $VULKAN_VCPKG_PORTS; do ports+=("$port:$VCPKG_TRIPLET"); done
        echo "[build] vcpkg install ${ports[*]}"
        "$VCPKG_ROOT/vcpkg" install "${ports[@]}" --clean-after-build
    fi
    if [[ -z "$(find_qt)" ]]; then
        echo "[build] Installing Qt into ~/Qt ..."
        python3 "$REPO_ROOT/tools/qt/install-qt.py" --kit clang_64 --root "$HOME/Qt"
    fi
fi

command -v cmake >/dev/null || { echo '[build] ERROR: cmake not found. Run with "deps".' >&2; exit 1; }
command -v clang++ >/dev/null || { echo '[build] ERROR: clang++ not found. Run: xcode-select --install' >&2; exit 1; }

QT_ROOT="$(find_qt)"
[[ -n "$QT_ROOT" && -d "$QT_ROOT" ]] || {
    echo '[build] ERROR: Qt 6 was not found. Run with "deps", or set QT_ROOT.' >&2; exit 1; }

PREFIX="$QT_ROOT"
if command -v brew >/dev/null; then
    for pkg in curl libarchive zlib glfw jpeg-turbo openal-soft freetype; do
        d="$(brew --prefix "$pkg" 2>/dev/null || true)"
        [[ -n "$d" && -d "$d" ]] && PREFIX="$PREFIX;$d"
    done
fi
VULKAN_ARGS=()
if [[ -f "$VCPKG_INSTALLED/include/vk_mem_alloc.h" ]]; then
    PREFIX="$PREFIX;$VCPKG_INSTALLED"
    GLSLC="$VCPKG_INSTALLED/tools/shaderc/glslc"
    [[ -x "$GLSLC" ]] && VULKAN_ARGS+=("-DFRUITY_GLSLC=$GLSLC")
fi
if [[ "$DO_VULKAN" == 1 ]]; then
    VULKAN_ARGS+=(-DFRUITY_REQUIRE_VULKAN=ON)
    command -v brew >/dev/null && for pkg in $VULKAN_BREW_PKGS; do
        d="$(brew --prefix "$pkg" 2>/dev/null || true)"
        [[ -n "$d" && -d "$d" ]] && PREFIX="$PREFIX;$d"
    done
fi
[[ -n "${CMAKE_PREFIX_PATH:-}" ]] && PREFIX="$PREFIX;$CMAKE_PREFIX_PATH"

GEN_ARGS=()
command -v ninja >/dev/null && GEN_ARGS=(-G Ninja)
LAUNCHER_ARGS=()
command -v ccache >/dev/null && LAUNCHER_ARGS=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)

BUILD_DIR="$OUT_ROOT/macos-$CONFIG"

echo "[build] Repository : $REPO_ROOT"
echo "[build] Arch       : $(uname -m)"
echo "[build] Config     : $CONFIG"
echo "[build] Qt         : $QT_ROOT"
echo "[build] Vulkan     : $([[ "$DO_VULKAN" == 1 ]] && echo required || echo "optional${VULKAN_ARGS[*]:+ (VMA from vcpkg)}")"
echo "[build] Build dir  : $BUILD_DIR"
echo

if [[ "$DO_CLEAN" == 1 && -d "$BUILD_DIR" ]]; then
    echo "[build] Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if ! cmake -S "$REPO_ROOT" -B "$BUILD_DIR" ${GEN_ARGS[@]+"${GEN_ARGS[@]}"} \
        -DCMAKE_BUILD_TYPE="$CONFIG" \
        ${LAUNCHER_ARGS[@]+"${LAUNCHER_ARGS[@]}"} \
        ${VULKAN_ARGS[@]+"${VULKAN_ARGS[@]}"} \
        -DCMAKE_PREFIX_PATH="$PREFIX"; then
    echo
    echo '[build] ERROR: CMake configure failed. Missing packages? Try "build-cpp-macos.sh deps" (add "vulkan" for the Vulkan backend).' >&2
    exit 1
fi
if [[ "$CONFIGURE_ONLY" == 1 ]]; then echo '[build] Configure finished.'; exit 0; fi

if ! cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$BUILD_JOBS"; then
    echo; echo '[build] ERROR: build failed.' >&2; exit 1
fi

APP="$BUILD_DIR/FruityPrime.app"
echo
echo '[build] Build succeeded:'
[[ -d "$APP" ]] && echo "  $APP"

if [[ "$DO_RUN" == 1 ]]; then
    BIN="$APP/Contents/MacOS/FruityPrime"
    [[ -x "$BIN" ]] || BIN="$BUILD_DIR/FruityPrime"
    [[ -x "$BIN" ]] || { echo '[build] ERROR: FruityPrime binary not found.' >&2; exit 1; }
    echo "[build] Starting $BIN -launcher"
    (cd "$(dirname "$BIN")" && nohup "$BIN" -launcher >/dev/null 2>&1 &)
fi
