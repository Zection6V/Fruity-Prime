#!/usr/bin/env bash
# Double-click helper (Finder): starts the app build-cpp-macos.sh built.
#
# Usage: run-macos.command [config] [options...]
#   config  : Release (default), Debug, RelWithDebInfo, MinSizeRel
#   options : front-screen options, e.g. -rhi vulkan, -debuglog, -fullscreen
#   e.g.    run-macos.command Release -rhi vulkan
#
# Started through `open`, the way Finder starts it, so it runs exactly as a
# player's copy would: no inherited DYLD paths, and its own Vulkan loader search.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CONFIG=Release
case "$(echo "${1:-}" | tr '[:upper:]' '[:lower:]')" in
    release)        CONFIG=Release; shift ;;
    debug)          CONFIG=Debug; shift ;;
    relwithdebinfo) CONFIG=RelWithDebInfo; shift ;;
    minsizerel)     CONFIG=MinSizeRel; shift ;;
esac

APP="$SCRIPT_DIR/out/macos-$CONFIG/FruityPrime.app"
if [[ ! -d "$APP" ]]; then
    echo "Not built yet: $APP"
    echo "Run build-cpp-and-log.command (or build-cpp-macos.sh $CONFIG) first."
    read -r -p "Press Enter to close..." _
    exit 1
fi

echo "Starting $APP $*"
# A bare launch opens the front screen; with any argument the program wants
# a command, so name the front screen when options are given.
if [[ $# -gt 0 ]]; then
    open -n "$APP" --args -launcher "$@"
else
    open -n "$APP"
fi
