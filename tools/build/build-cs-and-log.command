#!/usr/bin/env bash
# Double-click helper (Finder): builds the non-Android C# solution and writes the
# full output to tools/build/out/build-cs-macos.log. Usage: [Debug|Release] (default Release)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
mkdir -p "$SCRIPT_DIR/out"
LOG="$SCRIPT_DIR/out/build-cs-macos.log"
CONFIG="${1:-Release}"
export PATH="/usr/local/share/dotnet:$HOME/.dotnet:/opt/homebrew/bin:/usr/local/bin:$PATH"
echo "Building C# solution ($CONFIG)... (log: $LOG)"
(cd "$REPO_ROOT" && dotnet build "src/MphRead.sln" -c "$CONFIG") > "$LOG" 2>&1
RC=$?
echo "exit code: $RC" >> "$LOG"
tail -n 40 "$LOG"
echo
echo "exit code: $RC"
read -r -p "Press Enter to close..." _
exit $RC
