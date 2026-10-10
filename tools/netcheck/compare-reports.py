#!/usr/bin/env python3
"""Cross-check -netcheck reports: each player against how everyone else saw them.

Every -netcheck client prints what its own player did ("netcheck A mine A
feature value") and what it saw of every other player ("netcheck B saw A
feature value"), with the thresholds that say a feature happened
("netcheck-needed feature value single|pairwise") and the features its hunter
cannot do ("netcheck-na A feature"). This pairs the two for the same player:
did what A did reach B?

A client's own report can only compare its tour with what it saw of someone
else's, which is two players, not one; this is the comparison that measures
synchronisation.

Verdict per (player, observer, feature):
  ok        the player did it, and the observer saw it
  FAIL      the player did it, and the observer never saw it
  not run   the player never did it -- nothing was tested, which is not a pass
  n/a       the player's hunter cannot do it
  info      pairwise features, printed but never judged

Exit code: 0 pass, 1 any FAIL, 2 no FAIL but something was not run.

usage: compare-reports.py client-output.txt [client-output.txt ...]
"""

import re
import sys
from collections import defaultdict

LINE = re.compile(r"^\s*netcheck (\S+) (mine|saw) (\S+) (\S+) (-?[0-9.]+)\s*$")
NEEDED = re.compile(r"^\s*netcheck-needed (\S+) ([0-9.]+) (single|pairwise)\s*$")
NOT_APPLICABLE = re.compile(r"^\s*netcheck-na (\S+) (\S+)\s*$")
HUNTER = re.compile(r"^\s*netcheck-hunter (\S+) (\S+)\s*$")


def read(paths):
    own = defaultdict(dict)                       # player -> feature -> value
    seen = defaultdict(lambda: defaultdict(dict))  # observer -> player -> feature -> value
    needed = {}                                   # feature -> (value, pairwise)
    not_applicable = defaultdict(set)             # player -> features
    hunters = {}
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as handle:
            for text in handle:
                if m := NEEDED.match(text):
                    needed[m.group(1)] = (float(m.group(2)), m.group(3) == "pairwise")
                elif m := NOT_APPLICABLE.match(text):
                    not_applicable[m.group(1)].add(m.group(2))
                elif m := HUNTER.match(text):
                    hunters[m.group(1)] = m.group(2)
                elif m := LINE.match(text):
                    reporter, kind, subject, feature, value = m.groups()
                    if kind == "mine" and reporter == subject:
                        own[subject][feature] = float(value)
                    elif kind == "saw":
                        seen[reporter][subject][feature] = float(value)
    return own, seen, needed, not_applicable, hunters


def main(paths):
    own, seen, needed, not_applicable, hunters = read(paths)
    if not needed or not own:
        print("no netcheck report found in", ", ".join(paths))
        return 1
    failures = 0
    not_run = 0
    compared = 0
    for player in sorted(own):
        for observer in sorted(seen):
            if observer == player or player not in seen[observer]:
                continue
            print(f"--- {player} ({hunters.get(player, '?')}) as {observer} saw them ---")
            for feature, (threshold, pairwise) in needed.items():
                did = own[player].get(feature, 0.0)
                saw = seen[observer][player].get(feature, 0.0)
                if feature in not_applicable[player]:
                    verdict = "n/a"
                elif pairwise:
                    verdict = "info"
                elif did < threshold:
                    verdict = "not run"
                    not_run += 1
                elif saw >= threshold:
                    verdict = "ok"
                    compared += 1
                else:
                    verdict = "FAIL"
                    failures += 1
                    compared += 1
                ratio = f"{saw / did:6.0%}" if did > 0 and verdict in ("ok", "FAIL") else "      "
                print(f"  {feature:16} did {did:9.1f}  saw {saw:9.1f}  {ratio}  {verdict}")
    if compared == 0 and not_run == 0:
        print("nobody saw anybody: run at least two clients and pass every output")
        return 1
    if failures:
        print(f"RESULT: FAIL -- {failures} feature(s) a player did never reached another")
        return 1
    if not_run:
        print(f"RESULT: INCOMPLETE -- {compared} check(s) ok, {not_run} not run (not a pass)")
        return 2
    print(f"RESULT: PASS -- {compared} check(s) ok")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    sys.exit(main(sys.argv[1:]))
