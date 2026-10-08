#!/usr/bin/env bash
# One hit-location run on this machine: a native server and two headless
# native clients on the loopback, the line made up by the clients (-netlag).
#
#   hitloc-local.sh <label> <map> <mode: wells|lanes|all> <seconds>
#
#   HITLOC_BIN    directory holding FruityPrime, paths.txt and maps/ (required)
#   HITLOC_OUT    where runs land (default ~/fp-hitloc/runs)
#   HITLOC_LAG    each client's -netlag (default 250:40, the Japan line's shape)
#   HITLOC_LOSS   each client's -netloss percentage (default 0)
#   HITLOC_PORT   loopback port (default 28190)
#   HITLOC_SERVER_EXTRA / HITLOC_CLIENT_EXTRA   more flags for either side
#
# Everything goes through -headless: no window and no GPU, so a run cannot be
# lost to a driver, and it measures nothing but the netcode.
set -u
LABEL="${1:?label}"; MAP="${2:?map}"; MODE="${3:?mode}"; SECS="${4:?seconds}"
BIN="${HITLOC_BIN:?set HITLOC_BIN}"
OUT="${HITLOC_OUT:-$HOME/fp-hitloc/runs}/$LABEL"
PORT="${HITLOC_PORT:-28190}"
LAG="${HITLOC_LAG:-250:40}"
LOSS="${HITLOC_LOSS:-0}"
mkdir -p "$OUT"
cd "$BIN" || exit 1
export ALSOFT_DRIVERS=null
printf '%s | Battle | 60 | 999\n' "$MAP" > "$OUT/maprotation.txt"

./FruityPrime -server -port "$PORT" -players 8 -nomaster -noautoupdate \
  -rotation "$OUT/maprotation.txt" -hitlog "$OUT/hl-server.csv" ${HITLOC_SERVER_EXTRA:-} \
  > "$OUT/server.log" 2>&1 &
SERVER=$!
sleep 8

client() {
  ./FruityPrime -netcheck 127.0.0.1 -port "$PORT" -name "$1" -hunter Samus -seconds "$SECS" \
    -hitrig "$MODE" -headless -netlag "$LAG" -netloss "$LOSS" -hitlog "$OUT/hl-$1.csv" \
    ${HITLOC_CLIENT_EXTRA:-} > "$OUT/$1.log" 2>&1
}
client ALPHA &
A=$!
sleep 2
client BRAVO &
B=$!
wait "$A" "$B"
sleep 2
kill "$SERVER" 2>/dev/null
wait "$SERVER" 2>/dev/null
echo "== $LABEL ($MAP, -hitrig $MODE, ${SECS}s, -netlag $LAG) -> $OUT"
grep -h "hit rig:" "$OUT/ALPHA.log" "$OUT/BRAVO.log"
