#!/usr/bin/env python3
"""Cross-check -netcheck reports: each player against how everyone else saw them.

Every -netcheck client prints what its own player did over the whole run
("netcheck A mine A feature value"), what it did while each other player was
in the match ("netcheck A mine-with B feature value"), what it saw of every
other player ("netcheck B saw A feature value"), and what became of every
shot event it received from each of them ("netcheck-shots B from A ...").
The thresholds are the client's, printed with the report: how much of a
feature counts as having happened ("netcheck-needed"), how closely an
observer must agree ("netcheck-parity feature fraction absolute"), and the
features a hunter cannot do ("netcheck-na"). This pairs each player with
their own record: did what A did reach B, and did it arrive intact?

Two separate questions, judged separately:
  coverage  did the player do it, and did the observer see it at all
  parity    does what the observer saw agree with what the player did, over
            the time both were in the match, within the feature's tolerance

Verdicts per (player, observer, feature):
  ok        covered, and (where a tolerance is set) in agreement
  FAIL      the player did it and the observer never saw it
  MISMATCH  the observer saw it, but not as much as was done (or more)
  not run   the player never did it -- nothing was tested, which is not a pass
  n/a       the player's hunter cannot do it
  info      pairwise features, printed but never judged

Shot events are exact: every event an observer received has to have been
fired by its copy of the shooter. Events still waiting or awaited when the
run ended are the tail, reported and not judged.

Exit code: 0 pass, 1 any FAIL or MISMATCH, 2 no failure but something was
not run.

usage: compare-reports.py client-output.txt [client-output.txt ...]
"""

import re
import sys
from collections import defaultdict

LINE = re.compile(r"^\s*netcheck (\S+) (mine|mine-with|saw) (\S+) (\S+) (-?[0-9.]+)\s*$")
NEEDED = re.compile(r"^\s*netcheck-needed (\S+) ([0-9.]+) (single|pairwise)\s*$")
PARITY = re.compile(r"^\s*netcheck-parity (\S+) ([0-9.]+) ([0-9.]+)\s*$")
NOT_APPLICABLE = re.compile(r"^\s*netcheck-na (\S+) (\S+)\s*$")
HUNTER = re.compile(r"^\s*netcheck-hunter (\S+) (\S+)\s*$")
SHOTS = re.compile(r"^\s*netcheck-shots (\S+) from (\S+) (.*)$")


class Reports:
    def __init__(self):
        self.own = defaultdict(dict)                        # player -> feature -> value
        self.own_with = defaultdict(lambda: defaultdict(dict))  # player -> observer -> feature -> value
        self.seen = defaultdict(lambda: defaultdict(dict))  # observer -> player -> feature -> value
        self.needed = {}                                    # feature -> (value, pairwise)
        self.parity = {}                                    # feature -> (fraction, absolute)
        self.not_applicable = defaultdict(set)              # player -> features
        self.hunters = {}
        self.shots = defaultdict(dict)                      # observer -> shooter -> counts

    def read(self, path):
        with open(path, encoding="utf-8", errors="replace") as handle:
            for text in handle:
                if m := NEEDED.match(text):
                    self.needed[m.group(1)] = (float(m.group(2)), m.group(3) == "pairwise")
                elif m := PARITY.match(text):
                    self.parity[m.group(1)] = (float(m.group(2)), float(m.group(3)))
                elif m := NOT_APPLICABLE.match(text):
                    self.not_applicable[m.group(1)].add(m.group(2))
                elif m := HUNTER.match(text):
                    self.hunters[m.group(1)] = m.group(2)
                elif m := SHOTS.match(text):
                    words = m.group(3).split()
                    self.shots[m.group(1)][m.group(2)] = {
                        words[i]: int(words[i + 1]) for i in range(0, len(words) - 1, 2)}
                elif m := LINE.match(text):
                    reporter, kind, subject, feature, value = m.groups()
                    if kind == "mine" and reporter == subject:
                        self.own[subject][feature] = float(value)
                    elif kind == "mine-with":
                        self.own_with[reporter][subject][feature] = float(value)
                    elif kind == "saw":
                        self.seen[reporter][subject][feature] = float(value)


class Tally:
    def __init__(self):
        self.covered = 0
        self.agreed = 0
        self.failures = 0
        self.not_run = 0


def judge_feature(reports, tally, player, observer, feature):
    threshold, pairwise = reports.needed[feature]
    window = reports.own_with[player].get(observer)
    did = (window if window is not None else reports.own[player]).get(feature, 0.0)
    saw = reports.seen[observer][player].get(feature, 0.0)
    parity = reports.parity.get(feature)
    allowed = None
    if feature in reports.not_applicable[player]:
        verdict = "n/a"
    elif pairwise:
        verdict = "info"
    elif did < threshold:
        verdict = "not run"
        tally.not_run += 1
    elif saw < threshold:
        verdict = "FAIL"
        tally.failures += 1
    else:
        tally.covered += 1
        verdict = "ok"
        if parity is not None:
            allowed = max(parity[1], parity[0] * did)
            if abs(saw - did) > allowed:
                verdict = "MISMATCH"
                tally.failures += 1
            else:
                tally.agreed += 1
    ratio = f"{saw / did:6.0%}" if did > 0 and verdict in ("ok", "FAIL", "MISMATCH") else "      "
    tolerance = f"+-{allowed:<7.1f}" if allowed is not None else " " * 9
    print(f"  {feature:16} did {did:9.1f}  saw {saw:9.1f}  {ratio} {tolerance} {verdict}")


def judge_shots(reports, tally, player, observer):
    counts = reports.shots.get(observer, {}).get(player)
    if counts is None:
        return
    unfired = sum(counts.get(k, 0) for k in ("stale", "pushed", "abandoned", "lost"))
    received = counts.get("received", 0)
    if received == 0 and counts.get("gaps", 0) == 0:
        verdict = "not run"
        tally.not_run += 1
    elif unfired == 0:
        verdict = "ok"
        tally.agreed += 1
    else:
        verdict = "MISMATCH"
        tally.failures += 1
    print(f"  shot events      received {received}, fired {counts.get('fired', 0)}"
          f" ({counts.get('late', 0)} after a newer one); not fired: stale {counts.get('stale', 0)},"
          f" pushed out {counts.get('pushed', 0)}, abandoned {counts.get('abandoned', 0)},"
          f" never arrived {counts.get('lost', 0)} (recovered late {counts.get('recovered', 0)});"
          f" tail: waiting {counts.get('waiting', 0)}, awaited {counts.get('pending', 0)}  {verdict}")


def main(paths):
    reports = Reports()
    for path in paths:
        reports.read(path)
    if not reports.needed or not reports.own:
        print("no netcheck report found in", ", ".join(paths))
        return 1
    tally = Tally()
    pairs = 0
    for player in sorted(reports.own):
        for observer in sorted(reports.seen):
            if observer == player or player not in reports.seen[observer]:
                continue
            pairs += 1
            window = "while both were in the match" if observer in reports.own_with[player] \
                else "whole run: no window reported"
            print(f"--- {player} ({reports.hunters.get(player, '?')}) as {observer} saw them, {window} ---")
            for feature in reports.needed:
                judge_feature(reports, tally, player, observer, feature)
            judge_shots(reports, tally, player, observer)
    if pairs == 0:
        print("nobody saw anybody: run at least two clients and pass every output")
        return 1
    summary = f"{tally.covered} covered, {tally.agreed} in agreement"
    if tally.failures:
        print(f"RESULT: FAIL -- {tally.failures} check(s) failed; {summary}")
        return 1
    if tally.not_run:
        print(f"RESULT: INCOMPLETE -- {summary}, {tally.not_run} not run (not a pass)")
        return 2
    print(f"RESULT: PASS -- {summary}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    sys.exit(main(sys.argv[1:]))
