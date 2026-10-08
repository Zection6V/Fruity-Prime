#!/usr/bin/env bash
# One hit-location run against a server on the Japan bench box, over the real
# line: two headless native clients here, the authority there.
#
#   hitloc-japan.sh <label> <base|dev> <map> <mode: wells|lanes|all> <seconds>
#
# The server is restarted with a one-map rotation first -- which is also what
# zeroes its counters and truncates its -hitlog -- and its log comes back
# beside the clients' as hl-server.csv. Deploy with hitloc-deploy.sh first.
#
#   HITLOC_BIN    directory holding the client FruityPrime, paths.txt, maps/ (required)
#   HITLOC_OUT    where runs land (default ~/fp-hitloc/runs)
#   HITLOC_CLIENT_EXTRA   more client flags
set -u
LABEL="${1:?label}"; SIDE="${2:?base|dev}"; MAP="${3:?map}"; MODE="${4:?mode}"; SECS="${5:?seconds}"
BIN="${HITLOC_BIN:?set HITLOC_BIN}"
OUT="${HITLOC_OUT:-$HOME/fp-hitloc/runs}/$LABEL"
HOST="${HITLOC_JP_HOST:-13.78.14.98}"
USER_="${HITLOC_JP_USER:-livetek}"
KEY="${HITLOC_JP_KEY:-$HOME/.ssh/fp_japan}"
case "$SIDE" in base) PORT=27896 ;; dev) PORT=27895 ;; *) echo "side: base|dev" >&2; exit 2 ;; esac
REMOTE="/opt/fruityprime-$SIDE"
SERVICE="fruityprime-$SIDE"
SSH=(ssh -i "$KEY" -o BatchMode=yes "$USER_@$HOST")
mkdir -p "$OUT"

"${SSH[@]}" "printf '%s | Battle | 60 | 999\n' '$MAP' | sudo -u fpserver tee $REMOTE/maprotation.txt >/dev/null \
  && sudo systemctl restart $SERVICE && sleep 8 && systemctl is-active $SERVICE" | tail -1

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

"${SSH[@]}" "sudo cat $REMOTE/logs/hl-server.csv" > "$OUT/hl-server.csv"
"${SSH[@]}" "sudo journalctl -u $SERVICE --no-pager --since '-$(( SECS + 60 )) seconds'" > "$OUT/authority.log"
echo "== $LABEL ($SIDE:$PORT, $MAP, -hitrig $MODE, ${SECS}s) -> $OUT"
grep -h "hit rig:" "$OUT/ALPHA.log" "$OUT/BRAVO.log"
grep -h "round trip\|rtt" "$OUT/ALPHA.log" | head -2
