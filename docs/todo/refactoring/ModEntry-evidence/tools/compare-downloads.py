"""Compare saved download outputs and payloads, preserving the initial failure."""
import hashlib
import json
from pathlib import Path
import re


root = Path(__file__).resolve().parents[1] / "runs/downloads"
initial_results = (root / "initial-results.txt").read_text(encoding="utf-8-sig")


def initial_exit(label, command):
    return int(re.search(rf"{label} {command} exit=(\d+)", initial_results).group(1))


def payloads(label):
    return {p.name: {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
            for p in sorted((root / f"maps-{label}/downloaded").glob("*.pk3"))}


def output(command, label):
    return [line for line in (root / f"{command}-{label}.out.txt").read_text(encoding="utf-8-sig").splitlines()
            if not line.startswith("[debug]")]


baseline = payloads("me00-baseline")
final = payloads("me07b")
common = baseline.keys() & final.keys()
different = [name for name in sorted(common) if baseline[name] != final[name]]
report = {
    "initial_exit_source": "initial-results.txt (download tool result from supplied handoff JSONL)",
    "installserver": {"baseline_exit": initial_exit("me00-baseline", "installserver"),
                      "final_exit": initial_exit("me07b", "installserver"),
                      "normalized_stdout_equal": output("installserver", "me00-baseline") == output("installserver", "me07b"),
                      "server_directory_exists": (Path(__file__).resolve().parents[5] / "tools/build/out/msvc-Release/server").exists()},
    "mapsync_initial": {"baseline_exit": initial_exit("me00-baseline", "mapsync"),
                        "final_exit": initial_exit("me07b", "mapsync"),
                        "baseline_downloaded": 19, "final_downloaded": 20,
                        "baseline_failed": "mario.pk3",
                        "original_common_payloads_equal": all(baseline[name] == final[name] for name in common if name != "mario.pk3")},
    "mapsync_baseline_retry": {
        "exit": int((root / "mapsync-me00-baseline-retry.code.txt").read_text().strip().removeprefix("exit=")),
        "downloaded": re.findall(r"\[maps\] downloaded (.+)", "\n".join(output("mapsync", "me00-baseline-retry")))},
    "baseline_payloads": baseline, "final_payloads": final,
    "payloads_equal_after_retry": baseline == final,
    "different": different,
}
(root / "comparison.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(f"installserver: same stdout={report['installserver']['normalized_stdout_equal']}, exits=1/1")
print(f"mapsync: initially 19/20 vs 20/20; baseline retry={report['mapsync_baseline_retry']}")
print(f"payloads after retry: {len(common)} identical, {len(different)} different; full set equal={baseline == final}")
assert report["installserver"]["normalized_stdout_equal"]
assert report["mapsync_baseline_retry"]["exit"] == 0
assert baseline == final and len(baseline) == 20
