"""Compare real process exits without retrying or masking access violations."""
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


def main():
    repo = Path(__file__).resolve().parents[5]
    out = repo / "tools/build/out/msvc-Release"
    evidence = Path(__file__).resolve().parents[1] / "runs/exit-stress"
    evidence.mkdir(exist_ok=True)
    label = sys.argv[1]
    exe = out / ("FruityPrime.exe" if label == "fix" else "FruityPrime-me00-baseline.exe")
    results = {"exe": str(exe), "sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "commands": {}}
    for flag in ("-q3shaders", "-q3maps", "-mapitems"):
        counts = collections.Counter()
        runs = []
        for i in range(60):
            started = time.monotonic()
            p = subprocess.run([str(exe), flag, "missing.pk3", "-noupdate"], cwd=out,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
            code = p.returncode & 0xFFFFFFFF
            counts[f"0x{code:08x}"] += 1
            runs.append({"run": i + 1, "exit": f"0x{code:08x}", "seconds": round(time.monotonic() - started, 4)})
            if i == 0 or code == 0xC0000005:
                (evidence / f"{label}-{flag[1:]}-{i + 1}.stdout.txt").write_bytes(p.stdout)
                (evidence / f"{label}-{flag[1:]}-{i + 1}.stderr.txt").write_bytes(p.stderr)
        results["commands"][flag] = {"exit_counts": dict(counts), "runs": runs}
        print(label, flag, dict(counts), flush=True)
        (evidence / f"{label}.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    if label == "fix" and any(set(v["exit_counts"]) != {"0x00000001"} for v in results["commands"].values()):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
