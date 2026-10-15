"""Compare two server-loopback.py runs.

    python -I compare-server-loopback.py runs/baseline-server runs/<label>-server

The report (every exit code, liveness before the stop, exit after Ctrl+C)
must be identical. Of the text, what is compared is what the server CLI
decides: the directory's and the server's own lines, what -servers and
-hosts print, the refusal, and the client's join milestones. Clock times,
round-trip milliseconds, the client's ephemeral port and its frame statistics are measurements
and are masked or left out.
"""
import pathlib
import re
import sys

CLOCK = re.compile(r"^\[\d\d:\d\d:\d\d(\.\d+)?\] ")
STAMP = re.compile(r"\d{8}-\d{6}-\d+")
MS = re.compile(r"\b\d+(\.\d+)? ms\b")
# How many beams the bot fired depends on when the clients joined in wall
# time: measured at 18, 10 and 5 across runs of the same baseline binary.
BOT_SHOTS = re.compile(r"(shots spawned here \(slot: beams\):|power-ups:) [^(]*")
# The client's own source port; the rig's fixed ports (27991-27993) stay.
EPHEMERAL = re.compile(r"127\.0\.0\.1:(?!2799\d\b)\d+")
SERVER_LINES = re.compile(r"^\[(server|master)\]|^\[net\] (-simulate|server map)")
CLIENT_LINES = re.compile(r"^\[net\] (joining|joined|server map|connected)|^\[netcheck\] NC (runs|joined|could)")


def lines(path: pathlib.Path, keep=None) -> list[str]:
    result = []
    for raw in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        line = CLOCK.sub("", raw)
        line = BOT_SHOTS.sub(r"\1 <N>", line)
        line = EPHEMERAL.sub("127.0.0.1:<PORT>", MS.sub("<MS>", STAMP.sub("<STAMP>", line)))
        if line.startswith("[debug]") or (keep is not None and not keep.search(line)):
            continue
        result.append(line)
    return result


def main() -> int:
    left, right = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    checks = [("report.txt", None), ("refusal-hostports.out.txt", None), ("servers.out.txt", None),
              ("servers-after.out.txt", None), ("hosts.out.txt", None),
              ("netcheck.out.txt", CLIENT_LINES), ("server.log", SERVER_LINES), ("master.log", SERVER_LINES)]
    failures = 0
    for name, keep in checks:
        a, b = lines(left / name, keep), lines(right / name, keep)
        if a == b:
            print(f"SAME    {name} ({len(a)} line(s))")
            continue
        failures += 1
        first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
        print(f"DIFF    {name} at line {first + 1}: "
              f"{a[first] if first < len(a) else '<eof>'!r} vs {b[first] if first < len(b) else '<eof>'!r}")
    print(f"\n{len(checks) - failures} same, {failures} different")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
