"""Compile ServerCommands.cpp (and ModEntry.cpp) once more with MPHREAD_SERVER
defined, syntax only (/Zs), using the exact command ninja uses."""
import pathlib
import re
import subprocess
import sys

here = pathlib.Path(sys.argv[1])
status = 0
for name in (sys.argv[2:] or ["ServerCommands", "ModEntry"]):
    lines = [l for l in (here / f"cmd-{name}.txt").read_text(encoding="utf-8", errors="replace").splitlines() if "cl.exe" in l.lower()]
    if not lines:
        print(f"{name}: no compile command found")
        status = 1
        continue
    cmd = lines[-1].strip()
    # the obj output and the pdb are irrelevant for a syntax check
    cmd = re.sub(r"/Fo\S+", "", cmd)
    cmd = re.sub(r"/Fd\S+", "", cmd)
    cmd = re.sub(r"/FS", "", cmd)
    cmd = cmd.replace(" /c ", " /Zs /DMPHREAD_SERVER /c ", 1) if " /c " in cmd else cmd + " /Zs /DMPHREAD_SERVER"
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True, encoding="utf-8", errors="replace",
                       cwd=r"C:\Users\Admin\Documents\git\Fruity-Prime\tools\build\out\msvc-Release")
    errors = [l for l in (r.stdout + r.stderr).splitlines() if " error " in l or "fatal error" in l]
    print(f"{name}: exit={r.returncode} errors={len(errors)}")
    for e in errors[:10]:
        print("   ", e)
    status |= r.returncode != 0
sys.exit(status)
