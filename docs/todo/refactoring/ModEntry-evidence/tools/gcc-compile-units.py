"""Compile ModEntry.cpp, every Mods/Entry/*.cpp and the three ModEntry tests
with GCC (MSYS2 mingw64), using the exact commands the configured ninja tree
would run -- without its prerequisite check targets, which fail under GCC on
their own. Prints each unit's result and any diagnostic located in it.

    python -I gcc-compile-units.py <msys2 build dir> <repo>
"""
import os
import pathlib
import re
import subprocess
import sys

build, repo = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
# cc1plus exits 1 without a word when TEMP is a POSIX path (as it is when this
# runs from Git Bash), so give it a Windows one.
_temp = os.path.join(os.environ.get("LOCALAPPDATA", r"C:\Windows"), "Temp")
env = dict(os.environ, PATH=r"C:\msys64\mingw64\bin;" + os.environ["PATH"], TEMP=_temp, TMP=_temp)
units = ["CMakeFiles/fruity_mphread_native.dir/src/MphRead.Native/Mods/ModEntry.cpp.obj"]
units += [f"CMakeFiles/fruity_mphread_native.dir/src/MphRead.Native/Mods/Entry/{p.name}.obj"
          for p in sorted((repo / "src/MphRead.Native/Mods/Entry").glob("*.cpp"))]
units += [f"CMakeFiles/fruity_modentry_{n}_tests.dir/src/MphRead.Native/Testing/TestModEntry{t}.cpp.obj"
          for n, t in (("command_line", "CommandLine"), ("options", "Options"), ("values", "Values"))]
failures = 0
for unit in units:
    cmd = subprocess.run(["ninja", "-C", str(build), "-t", "commands", unit], capture_output=True,
                         text=True, env=env).stdout.strip().splitlines()[-1]
    # Never run from the build directory: it holds the game's runtime DLLs,
    # Windows searches the working directory for DLLs before PATH, and
    # cc1plus then loads the wrong one and exits 1 without a word. The
    # dependency-file flags are dropped and the object goes to a temp dir.
    args = cmd.split()
    i = args.index("-MD")
    args = args[:i] + args[i + 5:]
    out = pathlib.Path(_temp) / "modentry-gcc" / (pathlib.Path(unit).name + ".o")
    out.parent.mkdir(parents=True, exist_ok=True)
    args[args.index("-o") + 1] = str(out)
    r = subprocess.run(args, cwd=out.parent, env=env, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    name = unit.split("/MphRead.Native/")[1].removesuffix(".obj")
    own = [l for l in (r.stdout + r.stderr).splitlines()
           if re.search(re.escape(pathlib.Path(name).name) + r":\d+:\d+: (warning|error)", l)]
    print(f"{'OK  ' if r.returncode == 0 else 'FAIL'} {name}" + (f"  ({len(own)} diagnostic(s) in this file)" if own else ""))
    for line in own[:6]:
        print("     " + line.split("MphRead.Native/")[-1][:220])
    if r.returncode != 0 and not own:
        print("     " + (r.stdout + r.stderr)[-600:].replace("\n", "\n     "))
    failures += r.returncode != 0
print(f"\n{len(units) - failures}/{len(units)} compiled with {subprocess.run(['g++', '--version'], capture_output=True, text=True, env=env).stdout.splitlines()[0]}")
sys.exit(1 if failures else 0)
