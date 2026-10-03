#!/usr/bin/env bash
# Double-click helper (Finder): runs build-cpp-macos.sh and writes the full output to
# tools/build/out/build-cpp-macos.log. With no arguments it installs dependencies
# (MoltenVK and the vcpkg ports included), builds with Vulkan, and starts the game.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$SCRIPT_DIR/out"
LOG="$SCRIPT_DIR/out/build-cpp-macos.log"
if [[ $# -eq 0 ]]; then set -- deps vulkan run; fi
echo "Building... (log: $LOG)"
bash "$SCRIPT_DIR/build-cpp-macos.sh" "$@" > "$LOG" 2>&1
RC=$?
echo "exit code: $RC" >> "$LOG"
[[ $RC -ne 0 ]] && echo "BUILD FAILED: the FruityPrime.app in tools/build/out is from an earlier build." >> "$LOG"
tail -n 40 "$LOG"
echo
echo "exit code: $RC"
read -r -p "Press Enter to close..." _
exit $RC
