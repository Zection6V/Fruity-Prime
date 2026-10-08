#!/usr/bin/env bash
# The before/after comparison: both builds, every map, N rounds, interleaved
# so that whatever this machine is doing at the time weighs on both arms.
#
#   hitloc-ab.sh <rounds> <seconds> [map:tag:mode ...]
#
#   HITLOC_BASE / HITLOC_AFTER   the two run directories (FruityPrime, paths.txt, maps/)
#   HITLOC_PREFIX                label prefix (default ab)
#   plus everything hitloc-local.sh reads (HITLOC_LAG, HITLOC_LOSS, ...)
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROUNDS="${1:?rounds}"; SECS="${2:?seconds}"; shift 2
MAPS=("$@")
[ ${#MAPS[@]} -eq 0 ] && MAPS=("TEST WELLS:wells:wells" "TEST WELLS STILL:still:wells" "TEST LANES:lanes:lanes")
PREFIX="${HITLOC_PREFIX:-ab}"
OUT="${HITLOC_OUT:-$HOME/fp-hitloc/runs}"
for round in $(seq 1 "$ROUNDS"); do
  for entry in "${MAPS[@]}"; do
    IFS=: read -r map tag mode <<<"$entry"
    for arm in base after; do
      bin="HITLOC_BASE"; [ "$arm" = after ] && bin="HITLOC_AFTER"
      HITLOC_BIN="${!bin:?set $bin}" "$HERE/hitloc-local.sh" "$PREFIX-$arm-$tag-$round" "$map" "$mode" "$SECS"
    done
  done
done
echo
for entry in "${MAPS[@]}"; do
  IFS=: read -r map tag mode <<<"$entry"
  base=$(seq -s, -f "$OUT/$PREFIX-base-$tag-%g" 1 "$ROUNDS")
  after=$(seq -s, -f "$OUT/$PREFIX-after-$tag-%g" 1 "$ROUNDS")
  echo "######## $map"
  python3 "$HERE/hitloc.py" "before:$tag=$base" "after:$tag=$after"
done
