#!/usr/bin/env bash
# Interleaved A/B of two authority binaries on one remote box, every arm the
# same clients and the same rig (rewind-run.sh), then the table.
#
#   rewind-ab.sh <prefix> <rounds> <seconds> [pairs]
#
#   REWIND_BEFORE_BIN / REWIND_AFTER_BIN   binary names in REWIND_REMOTE_DIR
#   (default FruityPrime-before / FruityPrime-after); every other REWIND_*
#   setting is passed through to rewind-run.sh.
set -u
PREFIX="${1:?prefix}"; ROUNDS="${2:?rounds}"; SECS="${3:?seconds}"; PAIRS="${4:-4}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${REWIND_OUT:-$HOME/fp-rewind/runs}"
RUNS=()
for ((i = 1; i <= ROUNDS; i++)); do
  for arm in before after; do
    bin="${REWIND_BEFORE_BIN:-FruityPrime-before}"
    [ "$arm" = after ] && bin="${REWIND_AFTER_BIN:-FruityPrime-after}"
    REWIND_REMOTE_BIN="$bin" "$HERE/rewind-run.sh" "$PREFIX-$arm-$i" "$SECS" "$PAIRS"
    RUNS+=("$OUT/$PREFIX-$arm-$i")
  done
done
python3 -I "$HERE/rewind.py" "${RUNS[@]}"
