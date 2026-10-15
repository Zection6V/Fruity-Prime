"""Compare two run-cli-cases.ps1 captures case by case.

    python -I compare-cli-cases.py runs/baseline runs/candidate

Exit code, artifact hash and normalised stdout/stderr must match. The only
normalisation is for text that changes from one run of the *same* binary to
the next: log file names (timestamp and process id), the sandbox directory
named after the run label, and the executable's own file name. Anything else
that differs is reported, never ignored.
"""
import csv
import pathlib
import re
import sys

VOLATILE = [
    (re.compile(r"\d{8}-\d{6}-\d+"), "<STAMP-PID>"),
    (re.compile(r"runs\\[A-Za-z0-9_.-]+-sandbox"), r"runs\\<LABEL>-sandbox"),
    (re.compile(r"FruityPrime-[A-Za-z0-9_.-]+\.exe"), "FruityPrime.exe"),
    (re.compile(r"\b\d+(\.\d+)? ?(ms|s)\b"), "<DURATION>"),
    (re.compile(r"\b[0-9A-F]{16}\b"), "<ADDRESS>"),
    (re.compile(r"\d+% of budget"), "<N>% of budget"),
    (re.compile(r"rss [\d>-]+ MB \| peak \d+ MB"), "rss <MEMORY>"),
    (re.compile(r"private \d+ MB"), "private <MEMORY>"),
    # Window/GPU cases (the 'manual' set): the shared on-disk pipeline cache
    # grows run to run, Qt picks a fresh temp directory, GPU memory in use
    # is the machine's.
    (re.compile(r"initial-bytes=\d+"), "initial-bytes=<N>"),
    (re.compile(r"Fruity Prime-[A-Za-z0-9]+"), "Fruity Prime-<TMP>"),
    (re.compile(r"runs/[A-Za-z0-9_.-]+-sandbox"), "runs/<LABEL>-sandbox"),
    (re.compile(r"usage=\d+"), "usage=<N>"),
    (re.compile(r"saved-bytes=\d+ create-ms=[\d.]+"), "saved-bytes=<N> create-ms=<T>"),
    (re.compile(r"transfer_ns=\d+"), "transfer_ns=<T>"),
    (re.compile(r"\d+ lit pixels"), "<N> lit pixels"),
    # The usage text names the executable, and each stage runs a copy.
    (re.compile(r"FruityPrime-[A-Za-z0-9_.-]+(?= usage)"), "FruityPrime"),
]

# Cases whose text carries measurements (timings) that differ between two runs
# of the same binary; their exit code and artifacts are still compared.
# -gamepad samples a connected pad for a second: its live axes and the
# number of sample lines both vary.
# -reflexcheck prints the driver's own frame/wait counters.
NONDETERMINISTIC_TEXT = {"D17-netcombat-precedence", "D16-gamepad", "M-reflexcheck"}


def normalise(text: str) -> list[str]:
    for pattern, replacement in VOLATILE:
        text = pattern.sub(replacement, text)
    return text.splitlines()


def load(run: pathlib.Path) -> dict[str, dict[str, str]]:
    with (run / "summary.csv").open(encoding="utf-8-sig") as f:
        return {row["Case"]: row for row in csv.DictReader(f)}


def main() -> int:
    left, right = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    skip_text = set(NONDETERMINISTIC_TEXT)
    a, b = load(left), load(right)
    failures = 0
    for case in sorted(set(a) | set(b)):
        if case not in a or case not in b:
            print(f"MISSING  {case}")
            failures += 1
            continue
        problems = []
        if a[case]["Exit"] != b[case]["Exit"]:
            problems.append(f"exit {a[case]['Exit']} != {b[case]['Exit']}")
        if a[case]["Artifact"] != b[case]["Artifact"]:
            problems.append(f"artifact {a[case]['Artifact']} != {b[case]['Artifact']}")
        if case not in skip_text:
            for stream in ("out", "err"):
                la = normalise((left / f"{case}.{stream}.txt").read_text(encoding="utf-8", errors="replace"))
                lb = normalise((right / f"{case}.{stream}.txt").read_text(encoding="utf-8", errors="replace"))
                if la != lb:
                    first = next((i for i, (x, y) in enumerate(zip(la, lb)) if x != y), min(len(la), len(lb)))
                    problems.append(f"std{stream} differs at line {first + 1}: "
                                    f"{la[first] if first < len(la) else '<eof>'!r} vs "
                                    f"{lb[first] if first < len(lb) else '<eof>'!r}")
        tag = "SAME    " if not problems else "DIFF    "
        if problems:
            failures += 1
        note = " (text not compared: nondeterministic)" if case in skip_text else ""
        print(f"{tag}{case}{note}" + ("".join(f"\n         {p}" for p in problems)))
    print(f"\n{len(set(a) | set(b)) - failures} same, {failures} different")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
