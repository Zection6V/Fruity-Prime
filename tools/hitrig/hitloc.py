#!/usr/bin/env python3
"""Where the shots landed, as each machine saw it.

    hitloc.py RUN_DIR [RUN_DIR...]          one table per run, side by side
    hitloc.py --json RUN_DIR                 the same numbers, machine-readable
    hitloc.py NAME=DIR1,DIR2 ...             several runs of one arm pooled as one column

A run directory holds the -hitlog CSVs of one match: one per client
(hl-<name>.csv) and the authority's (hl-server.csv, or any file whose rows
say auth=1). Every machine writes the same columns; who is who follows from
the row itself:

  shooter view   ev=hit  on a client whose own slot fired        (S)
  victim view    ev=hit  on a client whose own slot was hit      (V)
  victim miss    ev=miss on the victim's client                  (M)
  victim damage  ev=dmg  on the victim's client                  (D)
  authority sim  ev=hit  on the authority (its own copy)         (A)
  claim judged   ev=claim, verdict < 100                         (C)
  claim applied  ev=claim, verdict = 100                         (P)

S, V, M, A, C and P share the shot key (shooter, victim, beam, launch frame).
D carries no key -- the damage event has none -- and is paired with V on the
victim's own clock, attacker and weapon.
"""
import csv
import glob
import json
import math
import os
import statistics
import sys
from collections import defaultdict

BEAMS = ["PowerBeam", "VoltDriver", "Missile", "Battlehammer", "Imperialist",
         "Judicator", "Magmaul", "ShockCoil", "OmegaCannon"]
# The headshot band starts 0.3 below the capsule's top: 81.25% of its height.
HEAD_PCT = 81.25
# A drawn impact and the damage it belongs to, on the victim's own clock.
SEEN_WINDOW_MS = 750


def beam_name(b):
    b = int(b)
    return BEAMS[b] if 0 <= b < len(BEAMS) else ("none" if b in (255, -1) else f"beam{b}")


def wrap(angle):
    while angle > 180:
        angle -= 360
    while angle < -180:
        angle += 360
    return angle


def load(run):
    rows = []
    for directory in run.split(","):
        rows += load_one(directory)
    return rows


def load_one(run):
    rows = []
    for path in sorted(glob.glob(os.path.join(run, "hl-*.csv"))):
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                if r.get("ev") in (None, "ev"):
                    continue
                r["_file"] = path
                for k in ("wall_ms", "frame", "snap", "auth", "local", "shooter", "victim", "beam",
                          "launch", "splash", "hs", "dmg", "alt"):
                    try:
                        r[k] = int(r[k])
                    except (TypeError, ValueError):
                        r[k] = 0
                for k in ("px", "py", "pz", "vx", "vy", "vz", "fx", "fz", "dy", "hpct", "ang", "rad"):
                    try:
                        r[k] = float(r[k])
                    except (TypeError, ValueError):
                        r[k] = float("nan")
                rows.append(r)
    return rows


def classify(rows):
    out = defaultdict(list)
    for r in rows:
        ev, auth, local = r["ev"], r["auth"], r["local"]
        if auth:
            if ev == "hit":
                out["A"].append(r)
            elif ev == "claim":
                verdict = int(r["extra"].split(";")[3]) if r["extra"].count(";") >= 3 else -1
                r["verdict"] = verdict
                if verdict == 100:
                    out["P"].append(r)
                else:
                    if r["extra"].split(";")[0] != "na":
                        hx, hy, hz = (float(v) for v in r["extra"].split(";")[:3])
                        r["hist"] = (hx, hy, hz)
                    out["C"].append(r)
            continue
        parts = r["extra"].split(";") if ev == "hit" else []
        r["blocked"] = len(parts) > 1 and parts[1] == "1"
        r["synth"] = len(parts) > 2 and parts[2] == "synth"
        if ev == "hit" and r["shooter"] == local and r["victim"] != local:
            # A hit the victim's invulnerability swallowed deals nothing and
            # is claimed by nobody: counted, and left out of every pairing.
            out["Sblocked" if r["blocked"] else "S"].append(r)
        elif ev == "hit" and r["victim"] == local and r["shooter"] != local:
            out["V"].append(r)
        elif ev == "miss" and r["victim"] == local:
            r["gap"] = float(r["extra"]) if r["extra"] else float("nan")
            out["M"].append(r)
        elif ev == "dmg" and r["victim"] == local:
            out["D"].append(r)
    return out


def key(r):
    return (r["shooter"], r["victim"], r["beam"], r["launch"])


def pair(left, right, tolerance=0, same_splash=True):
    """Pair rows on the shot key, in order of arrival within a key; with a
    tolerance, an unpaired left row may take a right row whose launch frame is
    within that many frames. Returns [(l, r or None)]."""
    index = defaultdict(list)
    for r in sorted(right, key=lambda r: r["wall_ms"]):
        index[key(r)].append(r)
    used = set()
    pairs = []
    pending = []
    for l in sorted(left, key=lambda r: r["wall_ms"]):
        match = None
        for r in index.get(key(l), []):
            if id(r) not in used and (not same_splash or r["splash"] == l["splash"]):
                match = r
                break
        if match is None:
            pending.append(l)
            continue
        used.add(id(match))
        pairs.append((l, match))
    for l in pending:
        match = None
        best = None
        for d in range(1, tolerance + 1):
            for launch in (l["launch"] - d, l["launch"] + d):
                for r in index.get((l["shooter"], l["victim"], l["beam"], launch), []):
                    if id(r) in used or (same_splash and r["splash"] != l["splash"]):
                        continue
                    gap = abs(r["wall_ms"] - l["wall_ms"])
                    if best is None or gap < best:
                        best, match = gap, r
            if match is not None:
                break
        if match is not None:
            used.add(id(match))
        pairs.append((l, match))
    return pairs


def stats(values):
    values = [v for v in values if v == v]
    if not values:
        return None
    values.sort()
    pick = lambda q: values[min(len(values) - 1, int(q * (len(values) - 1) + 0.5))]
    return {"n": len(values), "mean": statistics.fmean(values), "p50": pick(0.5),
            "p90": pick(0.9), "p99": pick(0.99), "max": values[-1]}


def analyse(run):
    name = None
    if "=" in run:
        name, run = run.split("=", 1)
    rows = load(run)
    c = classify(rows)
    per = defaultdict(lambda: defaultdict(list))
    total = defaultdict(list)

    def add(beam, name, value):
        per[beam][name].append(value)
        # The Shock Coil is one continuous beam dealing a tick every few
        # frames, each its own "hit" on the shooter's screen; it would swamp
        # every rate here, so it is reported on its own and kept out of these.
        if beam != "ShockCoil":
            total[name].append(value)

    # Shooter against victim: the same shot, drawn on the two screens.
    sv = pair([r for r in c["S"] if not r["splash"]], [r for r in c["V"] if not r["splash"]], tolerance=3)
    misses = defaultdict(list)
    for m in c["M"]:
        misses[key(m)].append(m)
    for s, v in sv:
        b = beam_name(s["beam"])
        add(b, "direct", 1)
        if v is not None:
            add(b, "seen_same_shot", 1)
            add(b, "d_hpct", abs(s["hpct"] - v["hpct"]))
            add(b, "d_dy", abs(s["dy"] - v["dy"]))
            add(b, "d_ang", abs(wrap(s["ang"] - v["ang"])))
            add(b, "hs_agree", 1 if s["hs"] == v["hs"] else 0)
            add(b, "band_agree", 1 if (s["hpct"] >= HEAD_PCT) == (v["hpct"] >= HEAD_PCT) else 0)
            add(b, "victim_late_ms", v["wall_ms"] - s["wall_ms"])
            add(b, "seen_as_flying_shot", 0 if v.get("synth") else 1)
        else:
            add(b, "seen_same_shot", 0)
            near = None
            for d in range(0, 4):
                for launch in {s["launch"] - d, s["launch"] + d}:
                    for m in misses.get((s["shooter"], s["victim"], s["beam"], launch), []):
                        if near is None or m["gap"] < near["gap"]:
                            near = m
                if near is not None:
                    break
            if near is not None:
                add(b, "miss_gap", near["gap"])
                add(b, "miss_found", 1)
            else:
                add(b, "miss_found", 0)
    for s in c["Sblocked"]:
        add(beam_name(s["beam"]), "blocked", 1)
    # Splash: same pairing, distance from the blast to the body instead.
    for s, v in pair([r for r in c["S"] if r["splash"]], [r for r in c["V"] if r["splash"]], tolerance=3):
        b = beam_name(s["beam"])
        add(b, "splash", 1)
        add(b, "splash_seen", 1 if v is not None else 0)

    # Shooter against the authority: the claim, judged and applied.
    judged = pair(c["S"], c["C"], tolerance=0, same_splash=False)
    for s, cl in judged:
        b = beam_name(s["beam"])
        add(b, "claimed", 1 if cl is not None else 0)
        if cl is None:
            continue
        add(b, "claim_ok", 1 if cl["verdict"] == 0 else 0)
        if "hist" in cl:
            hx, hy, hz = cl["hist"]
            add(b, "drawn_vs_history_y", abs(cl["px"] - hx) * 0 + abs(cl["py"] - hy))
            add(b, "drawn_vs_history_xz", math.hypot(cl["px"] - hx, cl["pz"] - hz))
    applied = pair(c["S"], c["P"], tolerance=0, same_splash=False)
    for s, p in applied:
        b = beam_name(s["beam"])
        add(b, "applied", 1 if p is not None else 0)
        if p is not None:
            add(b, "dmg_equal", 1 if p["dmg"] == s["dmg"] else 0)
    # The authority's own copy, which no longer counts: would it have agreed?
    shadow = pair([r for r in c["S"] if not r["splash"]], [r for r in c["A"] if not r["splash"]], tolerance=0)
    for s, a in shadow:
        b = beam_name(s["beam"])
        add(b, "shadow_agree", 1 if a is not None else 0)
        if a is not None:
            add(b, "shadow_d_hpct", abs(s["hpct"] - a["hpct"]))
            add(b, "shadow_hs_agree", 1 if s["hs"] == a["hs"] else 0)

    # The victim's own clock: damage against the impact drawn for it.
    by_victim = defaultdict(list)
    for v in c["V"]:
        by_victim[(v["_file"], v["shooter"], v["beam"])].append(v)
    used = set()
    for d in sorted(c["D"], key=lambda r: r["wall_ms"]):
        b = beam_name(d["beam"])
        add(b, "dmg_taken", 1)
        best = None
        for v in by_victim.get((d["_file"], d["shooter"], d["beam"]), []):
            if id(v) in used:
                continue
            gap = d["wall_ms"] - v["wall_ms"]
            if abs(gap) <= SEEN_WINDOW_MS and (best is None or abs(gap) < abs(best[0])):
                best = (gap, v)
        if best is None:
            add(b, "dmg_seen", 0)
        else:
            used.add(id(best[1]))
            add(b, "dmg_seen", 1)
            add(b, "impact_to_damage_ms", best[0])
    for v in c["V"]:
        b = beam_name(v["beam"])
        add(b, "impacts_drawn", 1)
        add(b, "impact_had_damage", 1 if id(v) in used else 0)

    def summarise(bucket):
        out = {}
        for name, values in bucket.items():
            if name in ("direct", "splash", "dmg_taken", "impacts_drawn", "blocked"):
                out[name] = len(values)
            elif name in ("seen_same_shot", "hs_agree", "band_agree", "claimed", "claim_ok", "applied",
                          "dmg_equal", "shadow_agree", "shadow_hs_agree", "dmg_seen", "impact_had_damage",
                          "splash_seen", "miss_found", "seen_as_flying_shot"):
                out[name] = {"n": len(values), "rate": sum(values) / len(values) if values else None}
            else:
                out[name] = stats(values)
        return out

    return {"run": name or run, "counts": {k: len(v) for k, v in c.items()},
            "total": summarise(total), "weapons": {b: summarise(v) for b, v in sorted(per.items())}}


ROWS = [
    ("shots that hit, shooter's view", "direct", "count"),
    ("  (more absorbed by invulnerability, not counted)", "blocked", "count"),
    ("  victim drew the same shot hitting", "seen_same_shot", "rate"),
    ("  of which the drawn shot itself arrived", "seen_as_flying_shot", "rate"),
    ("  |height| shooter vs victim, % body (p50)", "d_hpct", "p50"),
    ("  |height| shooter vs victim, % body (p90)", "d_hpct", "p90"),
    ("  |height| shooter vs victim, units (p90)", "d_dy", "p90"),
    ("  |bearing| shooter vs victim, deg (p50)", "d_ang", "p50"),
    ("  head/body agreement (game's call)", "hs_agree", "rate"),
    ("  head band agreement (geometry)", "band_agree", "rate"),
    ("  victim drew it later by, ms (p50)", "victim_late_ms", "p50"),
    ("  unseen: drawn shot passed at, units (p50)", "miss_gap", "p50"),
    ("  unseen: drawn shot passed at, units (p90)", "miss_gap", "p90"),
    ("damage taken, victim's view", "dmg_taken", "count"),
    ("  seen landing (impact drawn within 750 ms)", "dmg_seen", "rate"),
    ("  impact -> health drop, ms (p50)", "impact_to_damage_ms", "p50"),
    ("impacts drawn on the victim", "impacts_drawn", "count"),
    ("  that came with damage (precision)", "impact_had_damage", "rate"),
    ("claims judged / shooter hits", "claimed", "rate"),
    ("  accepted", "claim_ok", "rate"),
    ("  applied", "applied", "rate"),
    ("  damage applied = damage predicted", "dmg_equal", "rate"),
    ("  victim drawn vs authority history, Y (p90)", "drawn_vs_history_y", "p90"),
    ("  victim drawn vs authority history, XZ (p90)", "drawn_vs_history_xz", "p90"),
    ("authority's own copy agreed (shadow)", "shadow_agree", "rate"),
    ("  |height| shooter vs authority copy (p90)", "shadow_d_hpct", "p90"),
]


def cell(summary, name, field):
    value = summary.get(name)
    if value is None:
        return "-"
    if field == "count":
        return str(value)
    if field == "rate":
        return "-" if value["rate"] is None else f"{value['rate'] * 100:.1f}% ({value['n']})"
    return f"{value[field]:.2f}" if value and value.get(field) is not None else "-"


def print_table(results, scope="total"):
    names = [os.path.basename(r["run"].rstrip("/").split(",")[0]) for r in results]
    width = max(46, max(len(n) for n in names))
    print(f"{'':{width}}" + "".join(f"{n:>22}" for n in names))
    for label, name, field in ROWS:
        cells = [cell(r["total"] if scope == "total" else r["weapons"].get(scope, {}), name, field) for r in results]
        if all(c == "-" for c in cells):
            continue
        print(f"{label:{width}}" + "".join(f"{c:>22}" for c in cells))


def main(argv):
    as_json = "--json" in argv
    runs = [a for a in argv if not a.startswith("--")]
    if not runs:
        print(__doc__)
        return 2
    results = [analyse(r) for r in runs]
    if as_json:
        print(json.dumps(results, indent=1))
        return 0
    print("== all weapons but the Shock Coil (its own section below)")
    print_table(results)
    weapons = sorted({w for r in results for w in r["weapons"]})
    for w in weapons:
        print(f"\n== {w}")
        print_table(results, w)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
