#!/usr/bin/env python3
"""Check accepted starts and playout phases from production netlog traces."""

import argparse
import json
import math
from pathlib import Path
import re


def fields(line):
    return dict(re.findall(r"(\w+)=([^\s]+)", line))


def identity(row):
    return tuple(int(row[name]) for name in ("match", "epoch", "slot", "generation", "life", "id"))


def check(server, clients, allow_no_samples=False):
    authority = {}
    failures = []
    for line in Path(server).read_text(encoding="utf-8-sig", errors="replace").splitlines():
        if "[form-event] authority " in line:
            row = fields(line)
            key = identity(row)
            signature = (int(row["start"]), int(row["direction"]))
            if key in authority and authority[key] != signature:
                failures.append(f"authority changed event identity: {key}")
            authority[key] = signature
    if not authority:
        failures.append("no accepted authority transitions were exercised")

    results = []
    for path in clients:
        records = []
        stream = None
        seen = {}
        received = samples = 0
        directions = set()
        for line in Path(path).read_text(encoding="utf-8-sig", errors="replace").splitlines():
            if "[form-event] received " in line:
                row = fields(line)
                key = identity(row)
                current_stream = key[:2]
                if stream != current_stream:
                    records.clear()
                    stream = current_stream
                stamp = int(row["frame"])
                if (key, stamp) in seen and seen[(key, stamp)] != row:
                    failures.append(f"{path}: duplicate event changed its contents: {key}/{stamp}")
                # Lifecycle/presentation resets may receive the terminal
                # checkpoint again; it must remain identical and unplayable.
                seen[(key, stamp)] = row
                if authority.get(key) != (int(row["start"]), int(row["direction"])):
                    failures.append(f"{path}: event differs from authority: {key}")
                records.append({name: int(value) for name, value in row.items()})
                records = records[-512:]
                received += 1
                directions.add(int(row["direction"]))
            elif "[form-sample] " in line:
                row = fields(line)
                read = float(row["read"])
                phase = float(row["phase"])
                newest = int(row["newest"])
                candidates = [event for event in records
                              if all(event[name] == int(row[name]) for name in ("slot", "generation", "life"))
                              and event["start"] <= math.floor(read) and event["frame"] <= math.floor(read)]
                event = max(candidates, key=lambda event: event["frame"], default=None)
                if (not math.isfinite(read) or not math.isfinite(phase)
                        or read > newest + 1e-5 or not 0 <= phase <= 1 or event is None):
                    failures.append(f"{path}: invalid or unanchored playout phase: {row}")
                elif event["status"] != 0 or event["direction"] != int(row["direction"]):
                    failures.append(f"{path}: a terminal/opposite event replayed: {row}")
                else:
                    clip = event["clip"] + (0 if event["paused"] else max(0, read - event["anchor"]) / 2)
                    expected = max(0, min(1, clip / (event["frames"] - 1)))
                    if abs(phase - expected) > 2e-5:
                        failures.append(f"{path}: phase {phase} != shared-clock phase {expected}")
                samples += 1
        if not received or (not samples and not allow_no_samples):
            failures.append(f"{path}: event delivery/playout was not exercised")
        if directions != {1, 2}:
            failures.append(f"{path}: both morph and unmorph were not exercised")
        results.append({"path": str(path), "received": received, "samples": samples})
    return {"passed": not failures, "authority_transitions": len(authority), "clients": results, "failures": failures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server")
    parser.add_argument("clients", nargs="+")
    parser.add_argument("--allow-no-samples", action="store_true", help="for -nointerp / -noformevents comparison runs")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = check(args.server, args.clients, args.allow_no_samples)
    if args.json:
        args.json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
