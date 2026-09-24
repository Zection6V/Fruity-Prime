#!/usr/bin/env bash
# Every C#-to-C++ pitfall scanner, in turn. Each hit is a place to read.
# See docs/MphRead-Native-CSharp-to-Cpp-Pitfalls.md.
set -u
cd "$(dirname "$0")"
for scan in eval_order name_shadow slot_alias thread_local_dtor; do
    echo "== $scan"
    python3 "$scan.py"
done
