#!/usr/bin/env python3
"""What a rewind run (tools/hitrig/rewind-run.sh, -hitrig strafe) saw.

    rewind.py RUN [RUN...]          one column per run
    rewind.py --detail RUN          and every rewind, one line each

Each pair of clients (slots 2k, 2k+1) shoots only each other, so for a
victim V and its shooter S:

  bar rewinds       the shooter's bar for V went back up within one life
                    (hpup on S): a hit shown and taken back
  bar mismatches    a health value the shooter's bar showed for V that V's
                    own bar never showed in that life (and the converse:
                    two hits landing in one snapshot skip a value, which is
                    not a disagreement); final values that differ when the
                    life ended -- the one that matters
  puppet jumps      a step of a remote player drawn here that its previous
                    step does not explain: 'back' turned more than 120
                    degrees (a pull back), 'stall' restarted from standing,
                    'other' the rest. Teleports (respawn, rig placement)
                    are not counted.
  own jumps         the same for a client's own player (a correction)
  claims refused    the authority's verdicts other than applied, by reason

Every count is split by the weapon the shooter held at the time.
"""
import csv
import glob
import os
import sys
from collections import Counter, defaultdict

BEAMS = ["PowerBeam", "VoltDriver", "Missile", "Battlehammer", "Imperialist",
         "Judicator", "Magmaul", "ShockCoil", "OmegaCannon"]
VERDICTS = {0: "applied", 1: "duplicate", 2: "dead shooter", 3: "dead victim", 4: "refused",
            5: "too old", 6: "wrong life", 7: "geometry", 8: "damage limit", 9: "invalid launch",
            10: "no damage", 100: "applied", 201: "old shooter life", 202: "old victim life"}
TELEPORT = 3.0      # a step longer than this is a respawn or a placement
BACK_COS = -0.5     # turned more than 120 degrees
RUN_END_MS = 3000   # lives still going this close to the end of the run are left out


def beam_name(b):
    b = int(b)
    return BEAMS[b] if 0 <= b < len(BEAMS) else "none"


def read(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


class Client:
    def __init__(self, path):
        self.name = os.path.basename(path)[3:-4]
        self.rows = read(path)
        slots = Counter(int(r["local"]) for r in self.rows if r["local"] not in ("", "-1"))
        self.slot = slots.most_common(1)[0][0] if slots else -1
        # The weapon this client held, by wall clock: every watch row carries it.
        self.end_ms = max((int(r["wall_ms"]) for r in self.rows), default=0)
        self.weapon_at = sorted((int(r["wall_ms"]), int(r["beam"])) for r in self.rows
                                if r["ev"] in ("hp", "hpup", "jump") and int(r["beam"]) >= 0)

    def weapon(self, ms):
        lo, hi = 0, len(self.weapon_at)
        while lo < hi:
            mid = (lo + hi) // 2
            if self.weapon_at[mid][0] <= ms:
                lo = mid + 1
            else:
                hi = mid
        return self.weapon_at[lo - 1][1] if lo > 0 else -1


def health_by_life(client, slot):
    """life -> list of (wall_ms, old, new) for that slot as drawn on client."""
    lives = defaultdict(list)
    for r in client.rows:
        if r["ev"] in ("hp", "hpup") and int(r["victim"]) == slot:
            old, new, life = r["extra"].split(";")[:3]
            lives[int(life)].append((int(r["wall_ms"]), int(old), int(new)))
    return lives


def analyse(run):
    files = sorted(glob.glob(os.path.join(run, "hl-P*.csv")))
    clients = [Client(f) for f in files]
    by_slot = {c.slot: c for c in clients if c.slot >= 0}
    out = defaultdict(Counter)   # weapon -> counters
    detail = []

    def add(weapon, key, n=1):
        out[beam_name(weapon) if weapon >= 0 else "none"][key] += n
        out["all"][key] += n

    for v_slot, victim in by_slot.items():
        shooter = by_slot.get(v_slot ^ 1)
        if shooter is None:
            continue
        mine = health_by_life(victim, v_slot)
        seen = health_by_life(shooter, v_slot)
        for life, rows in seen.items():
            own = mine.get(life, [])
            # A life the run's end cut short is compared by neither machine.
            if max(rows[-1][0], own[-1][0] if own else 0) > min(victim.end_ms, shooter.end_ms) - RUN_END_MS:
                continue
            own_values = {new for _, _, new in own} | {old for _, old, _ in own}
            seen_values = {new for _, _, new in rows} | {old for _, old, _ in rows}
            for ms, old, new in rows:
                w = shooter.weapon(ms)
                add(w, "bar changes (shooter)")
                if new > old and old > 0:
                    add(w, "bar rewinds (shooter)")
                    detail.append(f"{os.path.basename(run)} {shooter.name} saw {victim.name} {old}->{new} "
                                  f"(life {life}, {beam_name(w)}) at {ms}")
                if new not in own_values and new > 0:
                    add(w, "shooter showed a value the victim never had")
            for ms, old, new in own:
                w = shooter.weapon(ms)
                add(w, "bar changes (victim)")
                if new > old and old > 0:
                    add(w, "bar rewinds (victim)")
                if new not in seen_values and new > 0:
                    add(w, "victim had a value the shooter never showed")
            if own and rows:
                add(shooter.weapon(rows[-1][0]), "lives compared")
                if own[-1][2] != rows[-1][2]:
                    add(shooter.weapon(rows[-1][0]), "lives ending on different health")
                    detail.append(f"{os.path.basename(run)} life {life} of {victim.name}: victim ends {own[-1][2]}, "
                                  f"{shooter.name} shows {rows[-1][2]}")

    for c in clients:
        for r in c.rows:
            if r["ev"] != "jump":
                continue
            residual, step, before, life, *rest = r["extra"].split(";")
            step, before = float(step), float(before)
            if step > TELEPORT or before > TELEPORT:
                continue
            turn = float(rest[0]) if rest else 1.0
            slot = int(r["victim"])
            kind = "back" if turn < BACK_COS and before > 0.02 else "stall" if before < 0.02 else "other"
            w = c.weapon(int(r["wall_ms"]))
            if slot == c.slot:
                # Our own player stepped off its motion: the shooter is the partner.
                add(by_slot[c.slot ^ 1].weapon(int(r["wall_ms"])) if (c.slot ^ 1) in by_slot else -1,
                    f"own jumps: {kind}")
            elif slot == c.slot ^ 1:
                add(w, f"puppet jumps: {kind}")
                if kind == "back":
                    detail.append(f"{os.path.basename(run)} {c.name} drew slot {slot} pulled back "
                                  f"{float(residual):.2f} (step {step:.2f}, turn {turn:.2f}, {beam_name(w)}) "
                                  f"at {r['wall_ms']}")

    server = os.path.join(run, "hl-server.csv")
    if os.path.exists(server):
        for r in read(server):
            if r["ev"] != "claim":
                continue
            parts = r["extra"].split(";")
            verdict = int(parts[3]) if len(parts) > 3 and parts[3].lstrip("-").isdigit() else -1
            if verdict == 100:
                add(int(r["beam"]), "claims applied")
            elif verdict != 0:
                add(int(r["beam"]), f"claims refused: {VERDICTS.get(verdict, verdict)}")
    for c in clients:
        for r in c.rows:
            if r["ev"] == "hit" and int(r["shooter"]) == c.slot and int(r["victim"]) == (c.slot ^ 1):
                add(int(r["beam"]), "hits predicted (shooter)")
            elif r["ev"] == "dmg" and int(r["victim"]) == c.slot:
                add(int(r["beam"]), "damage taken (victim)")
    return out, detail, [c.name for c in clients]


def main(argv):
    show = "--detail" in argv
    runs = [a for a in argv if not a.startswith("--")]
    if not runs:
        print(__doc__)
        return 2
    results = [analyse(r) for r in runs]
    names = [os.path.basename(r.rstrip("/")) for r in runs]
    weapons = ["all"] + [b for b in BEAMS if any(b in res[0] for res in results)]
    keys = sorted({k for res in results for w in res[0].values() for k in w})
    for w in weapons:
        print(f"\n## {w}")
        print(f"{'':52}" + "".join(f"{n:>18}" for n in names))
        for k in keys:
            vals = [res[0].get(w, Counter()).get(k, 0) for res in results]
            if any(vals):
                print(f"{k:52}" + "".join(f"{v:>18}" for v in vals))
    if show:
        for res in results:
            print()
            for line in res[1]:
                print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
