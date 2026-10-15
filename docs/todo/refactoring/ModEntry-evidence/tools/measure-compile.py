"""Time the real MSVC compile of ModEntry before (HEAD) and after (ModEntry.cpp
plus every Mods/Entry/*.cpp), with the exact commands ninja uses, objects
written to a scratch directory. Sequential, best of N runs per unit."""
import pathlib
import re
import subprocess
import sys
import tempfile
import time

OUT = pathlib.Path(r"C:\Users\Admin\Documents\git\Fruity-Prime\tools\build\out\msvc-Release")
MODS = pathlib.Path(r"C:\Users\Admin\Documents\git\Fruity-Prime\src\MphRead.Native\Mods")
RUNS = int(sys.argv[1]) if len(sys.argv) > 1 else 2


def command_for(rel_obj):
    r = subprocess.run(["ninja", "-C", str(OUT), "-t", "commands", rel_obj], capture_output=True, text=True)
    lines = [l for l in r.stdout.splitlines() if "cl.exe" in l.lower()]
    return lines[-1].strip()


def timed(cmd, obj_dir):
    cmd = re.sub(r"/Fo\S+", lambda _: f"/Fo{obj_dir}\\", cmd)
    cmd = re.sub(r"/Fd\S+", lambda _: f"/Fd{obj_dir}\\", cmd)
    best = None
    for _ in range(RUNS):
        start = time.perf_counter()
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=str(OUT))
        elapsed = time.perf_counter() - start
        if r.returncode != 0:
            raise SystemExit(f"compile failed:\n{r.stdout[-2000:]}")
        best = elapsed if best is None else min(best, elapsed)
    return best


base = command_for(r"CMakeFiles\fruity_mphread_native.dir\src\MphRead.Native\Mods\ModEntry.cpp.obj")
head_copy = MODS / "ZzBaselineModEntry.cpp"
head_copy.write_bytes(subprocess.run(["git", "-C", str(MODS), "show", "HEAD:src/MphRead.Native/Mods/ModEntry.cpp"],
                                     capture_output=True, check=True).stdout)
try:
    with tempfile.TemporaryDirectory() as objs:
        before = timed(base.replace("ModEntry.cpp", "ZzBaselineModEntry.cpp"), objs)
        after = {"ModEntry.cpp": timed(base, objs)}
        for cpp in sorted((MODS / "Entry").glob("*.cpp")):
            rel = rf"CMakeFiles\fruity_mphread_native.dir\src\MphRead.Native\Mods\Entry\{cpp.name}.obj"
            after[f"Entry/{cpp.name}"] = timed(command_for(rel), objs)
finally:
    head_copy.unlink()

print(f"before  ModEntry.cpp (HEAD)            {before:6.2f} s")
for name, seconds in after.items():
    print(f"after   {name:30s} {seconds:6.2f} s")
print(f"after   total ({len(after)} units)             {sum(after.values()):6.2f} s")
print(f"after   longest single unit           {max(after.values()):6.2f} s")
