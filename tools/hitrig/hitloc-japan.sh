#!/usr/bin/env bash
# One hit-location run against a server on the Japan bench box, over the real
# line: two headless native clients here, the authority there.
#
#   hitloc-japan.sh <label> <base|dev> <map> <mode: wells|lanes|all> <seconds>
#
# The server is started fresh with a one-map rotation for every run -- which
# is also what zeroes its counters and truncates its -hitlog -- and stopped
# afterwards, so the two arms never share the box; its log comes back beside
# the clients' as hl-server.csv. It runs as the login user straight out of
# /opt/fruityprime-<side> (no systemd, no sudo): the two bench units are
# disabled while this rig owns those directories.
#
#   HITLOC_BIN    directory holding the client FruityPrime, paths.txt, maps/ (required)
#   HITLOC_OUT    where runs land (default ~/fp-hitloc/runs)
#   HITLOC_CLIENT_EXTRA   more client flags
#   Another box: HITLOC_JP_HOST, HITLOC_REMOTE_PREFIX (default /opt/fruityprime-;
#   the side is appended), HITLOC_PORT_BASE / HITLOC_PORT_DEV (27896 / 27895).
set -u
LABEL="${1:?label}"; SIDE="${2:?base|dev}"; MAP="${3:?map}"; MODE="${4:?mode}"; SECS="${5:?seconds}"
BIN="${HITLOC_BIN:?set HITLOC_BIN}"
OUT="${HITLOC_OUT:-$HOME/fp-hitloc/runs}/$LABEL"
HOST="${HITLOC_JP_HOST:-13.78.14.98}"
USER_="${HITLOC_JP_USER:-livetek}"
KEY="${HITLOC_JP_KEY:-$HOME/.ssh/fp_japan}"
case "$SIDE" in base) PORT="${HITLOC_PORT_BASE:-27896}" ;; dev) PORT="${HITLOC_PORT_DEV:-27895}" ;; *) echo "side: base|dev" >&2; exit 2 ;; esac
REMOTE="${HITLOC_REMOTE_PREFIX:-/opt/fruityprime-}$SIDE"
SSH=(ssh -i "$KEY" "$USER_@$HOST")
mkdir -p "$OUT"

"${SSH[@]}" "R=$REMOTE; cd \$R || exit 1
  [ -f server.pid ] && kill \$(cat server.pid) 2>/dev/null && sleep 2
  printf '%s | Battle | 60 | 999\n' '$MAP' > maprotation.txt
  ALSOFT_DRIVERS=null setsid nohup ./FruityPrime -server -port $PORT -players 8 -nomaster -noautoupdate \
    -rotation \$R/maprotation.txt -hitlog \$R/logs/hl-server.csv > \$R/logs/server-console.log 2>&1 < /dev/null &
  echo \$! > server.pid; sleep 8; kill -0 \$(cat server.pid) && echo server up on $PORT"

cd "$BIN" || exit 1
export ALSOFT_DRIVERS=null
client() {
  ./FruityPrime -netcheck "$HOST" -port "$PORT" -name "$1" -hunter Samus -seconds "$SECS" \
    -hitrig "$MODE" -headless -hitlog "$OUT/hl-$1.csv" ${HITLOC_CLIENT_EXTRA:-} > "$OUT/$1.log" 2>&1
}
client ALPHA &
A=$!
sleep 2
client BRAVO &
B=$!
wait "$A" "$B"
sleep 2

"${SSH[@]}" "ps -o rss=,cputime=,etimes= -p \$(cat $REMOTE/server.pid)" > "$OUT/server-cost.txt" 2>/dev/null
"${SSH[@]}" "kill \$(cat $REMOTE/server.pid) 2>/dev/null; sleep 2; rm -f $REMOTE/server.pid"
"${SSH[@]}" "cat $REMOTE/logs/hl-server.csv" > "$OUT/hl-server.csv"
"${SSH[@]}" "cat $REMOTE/logs/server-console.log" > "$OUT/authority.log"
echo "== $LABEL ($SIDE:$PORT, $MAP, -hitrig $MODE, ${SECS}s) -> $OUT"
grep -h "hit rig:" "$OUT/ALPHA.log" "$OUT/BRAVO.log"
grep -h "round trip\|rtt" "$OUT/ALPHA.log" | head -2
