#!/usr/bin/env bash
# The before/after comparison over the real line: hitloc-japan.sh against the
# bench box's two unlisted servers (base = before, dev = after), every map,
# N rounds, interleaved, then the joined table per map.
#
#   hitloc-japan-ab.sh <rounds> <seconds> [map:tag:mode ...]
#
#   HITLOC_BIN_BASE / HITLOC_BIN_AFTER   client run directories, one per protocol
#   HITLOC_PREFIX                        label prefix (default jp)
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROUNDS="${1:?rounds}"; SECS="${2:?seconds}"; shift 2
MAPS=("$@")
[ ${#MAPS[@]} -eq 0 ] && MAPS=("TEST WELLS:wells:wells" "TEST WELLS STILL:still:wells" "TEST LANES:lanes:lanes")
PREFIX="${HITLOC_PREFIX:-jp}"
OUT="${HITLOC_OUT:-$HOME/fp-hitloc/runs}"
for round in $(seq 1 "$ROUNDS"); do
  for entry in "${MAPS[@]}"; do
    IFS=: read -r map tag mode <<<"$entry"
    HITLOC_BIN="${HITLOC_BIN_BASE:?}" "$HERE/hitloc-japan.sh" "$PREFIX-base-$tag-$round" base "$map" "$mode" "$SECS"
    HITLOC_BIN="${HITLOC_BIN_AFTER:?}" "$HERE/hitloc-japan.sh" "$PREFIX-after-$tag-$round" dev "$map" "$mode" "$SECS"
  done
done
echo
for entry in "${MAPS[@]}"; do
  IFS=: read -r map tag mode <<<"$entry"
  echo "######## $map (${HITLOC_JP_HOST:-13.78.14.98})"
  python3 "$HERE/hitloc.py" "before:$tag=$(seq -s, -f "$OUT/$PREFIX-base-$tag-%g" 1 "$ROUNDS")" \
    "after:$tag=$(seq -s, -f "$OUT/$PREFIX-after-$tag-%g" 1 "$ROUNDS")"
done
