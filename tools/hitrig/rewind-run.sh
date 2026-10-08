#!/usr/bin/env bash
# One rewind run: up to four pairs of headless native clients in one match on
# TEST STRAFE, each pair in its own walled corridor, both players of a pair
# shooting each other and strafing non-stop (-hitrig strafe). Every client
# and the authority write -hitlog; tools/hitrig/rewind.py reads the run.
#
#   rewind-run.sh <label> <seconds> [pairs, default 4]
#
#   REWIND_BIN     directory holding the client FruityPrime, paths.txt, maps/ (required)
#   REWIND_OUT     where runs land (default ~/fp-rewind/runs)
#   REWIND_RIG     -hitrig value (default strafe; strafe:missilevolt, strafe:missile ...)
#   REWIND_SPEED   -rigspeed (default 1.5)        REWIND_TURN  -rigstrafe (default 24)
#   REWIND_RANGE   -rigrange, units between the two of a pair (default 12)
#   REWIND_HUNTERS hunters given to A and B of every pair (default "Samus Samus";
#                  "Samus Kanden" puts both homing affinity weapons in play; eight
#                  names give each client its own, in join order)
#   REWIND_CLIENT_EXTRA / REWIND_SERVER_EXTRA   more flags for either side
#
# Local authority (default): a native server on the loopback, the line made
# up by the clients: REWIND_LAG (default 40:10), REWIND_LOSS (default 0),
# REWIND_PORT (default 28290).
#
# Remote authority: REWIND_HOST, REWIND_PORT, REWIND_REMOTE_DIR (a directory
# holding the server binary and maps/ on that box; REWIND_REMOTE_BIN names the
# binary there, default FruityPrime) and REWIND_SSH (the command that
# runs a shell line there, e.g. "ssh -i key user@host"). The server is started
# fresh for the run, unlisted, with a one-map rotation, and stopped afterwards;
# its log and -hitlog come back beside the clients'.
set -u
LABEL="${1:?label}"; SECS="${2:?seconds}"; PAIRS="${3:-4}"
BIN="${REWIND_BIN:?set REWIND_BIN}"
OUT="${REWIND_OUT:-$HOME/fp-rewind/runs}/$LABEL"
RIG="${REWIND_RIG:-strafe}"
SPEED="${REWIND_SPEED:-1.5}"
TURN="${REWIND_TURN:-24}"
RANGE="${REWIND_RANGE:-12}"
# Two names: A and B of every pair. More: one per client, in join order.
read -r -a HUNTERS <<< "${REWIND_HUNTERS:-Samus Samus}"
HUNTER_A="${HUNTERS[0]}"; HUNTER_B="${HUNTERS[1]:-$HUNTER_A}"
HOST="${REWIND_HOST:-127.0.0.1}"
mkdir -p "$OUT"
cd "$BIN" || exit 1
export ALSOFT_DRIVERS=null
MAP="TEST STRAFE"
# The match outlives the run: a rotation underneath it clears every counter.
ROT='TEST STRAFE | Battle | 60 | 999'

if [ "$HOST" = 127.0.0.1 ]; then
  PORT="${REWIND_PORT:-28290}"
  LAGFLAGS="-netlag ${REWIND_LAG:-40:10} -netloss ${REWIND_LOSS:-0}"
  printf '%s\n' "$ROT" > "$OUT/maprotation.txt"
  ./FruityPrime -server -port "$PORT" -players 8 -nomaster -noautoupdate \
    -rotation "$OUT/maprotation.txt" -hitlog "$OUT/hl-server.csv" ${REWIND_SERVER_EXTRA:-} \
    > "$OUT/server.log" 2>&1 &
  SERVER=$!
  sleep 8
else
  PORT="${REWIND_PORT:?set REWIND_PORT}"
  LAGFLAGS=""
  R="${REWIND_REMOTE_DIR:?set REWIND_REMOTE_DIR}"
  $REWIND_SSH "cd $R || exit 1
    [ -f server.pid ] && kill \$(cat server.pid) 2>/dev/null && sleep 2
    mkdir -p logs; printf '%s\n' '$ROT' > maprotation.txt; rm -f logs/hl-server.csv
    ALSOFT_DRIVERS=null HOME=$R setsid nohup ./${REWIND_REMOTE_BIN:-FruityPrime} -server -port $PORT -players 8 -nomaster -noautoupdate \
      -rotation $R/maprotation.txt -hitlog $R/logs/hl-server.csv ${REWIND_SERVER_EXTRA:-} \
      > $R/logs/server-console.log 2>&1 < /dev/null &
    echo \$! > server.pid; sleep 8; kill -0 \$(cat server.pid) && echo server up on $PORT"
fi

# Slots are given in the order the clients arrive: P0A P0B P1A P1B ... so
# slot / 2 is the pair and the corridor.
PIDS=()
for ((p = 0; p < PAIRS; p++)); do
  for side in A B; do
    name="P${p}${side}"
    hunter="$HUNTER_A"; [ "$side" = B ] && hunter="$HUNTER_B"
    idx=$((2 * p)); [ "$side" = B ] && idx=$((idx + 1))
    [ "${#HUNTERS[@]}" -gt 2 ] && hunter="${HUNTERS[$idx]:-$HUNTER_A}"
    ./FruityPrime -netcheck "$HOST" -port "$PORT" -name "$name" -hunter "$hunter" -seconds "$SECS" \
      -hitrig "$RIG" -rigspeed "$SPEED" -rigstrafe "$TURN" -rigrange "$RANGE" -headless $LAGFLAGS \
      -hitlog "$OUT/hl-$name.csv" ${REWIND_CLIENT_EXTRA:-} > "$OUT/$name.log" 2>&1 &
    PIDS+=($!)
    sleep 1.5
  done
done
wait "${PIDS[@]}"
sleep 2

if [ "$HOST" = 127.0.0.1 ]; then
  ps -o rss=,cputime=,etimes= -p "$SERVER" > "$OUT/server-cost.txt" 2>/dev/null
  kill "$SERVER" 2>/dev/null
  wait "$SERVER" 2>/dev/null
else
  $REWIND_SSH "cd $R; ps -o rss=,cputime=,etimes= -p \$(cat server.pid)" > "$OUT/server-cost.txt" 2>/dev/null
  $REWIND_SSH "cd $R; kill \$(cat server.pid) 2>/dev/null; sleep 2; rm -f server.pid"
  $REWIND_SSH "cat $R/logs/hl-server.csv" > "$OUT/hl-server.csv"
  $REWIND_SSH "cat $R/logs/server-console.log" > "$OUT/server.log"
fi
echo "== $LABEL ($HOST:$PORT, -hitrig $RIG x$SPEED range $RANGE, $HUNTER_A/$HUNTER_B, ${PAIRS} pairs, ${SECS}s) -> $OUT"
grep -h "hit rig:\|rewind watch:" "$OUT"/P*.log
