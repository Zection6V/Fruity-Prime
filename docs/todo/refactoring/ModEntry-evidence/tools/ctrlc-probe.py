import ctypes, os, subprocess, sys, time, pathlib

exe = sys.argv[1]
out = pathlib.Path(sys.argv[2])
if os.environ.get("INNER") != "1":
    p = subprocess.Popen([sys.executable, "-I", __file__, *sys.argv[1:]], env=dict(os.environ, INNER="1"),
                         creationflags=subprocess.CREATE_NEW_CONSOLE)
    sys.exit(p.wait())

results = []
for variant, extra, redirect in [("console", ["-console"], False), ("console+debuglog", ["-console", "-debuglog"], False), ("console-again", ["-console"], False)]:
    kw = {}
    if redirect:
        kw = dict(stdout=open(out / f"{variant}.out", "wb"), stderr=open(out / f"{variant}.err", "wb"))
    p = subprocess.Popen([exe, "-masterserver", "-port", "27995", "-hostports", "none", "-noupdate", "-noautoupdate", *extra],
                         stdin=subprocess.DEVNULL, **kw)
    time.sleep(3)
    k = ctypes.windll.kernel32
    k.SetConsoleCtrlHandler(None, True)
    sent = k.GenerateConsoleCtrlEvent(0, 0)
    try:
        code = p.wait(15)
    except subprocess.TimeoutExpired:
        p.kill(); code = "TIMEOUT"
    k.SetConsoleCtrlHandler(None, False)
    results.append(f"{variant}: sent={sent} exit={code}")
(out / "ctrlc-result.txt").write_text("\n".join(results), encoding="utf-8")
