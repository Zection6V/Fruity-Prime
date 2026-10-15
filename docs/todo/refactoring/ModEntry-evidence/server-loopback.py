"""ME-06 server scenario on the loopback, for one executable (Windows).

    python -I server-loopback.py <FruityPrime.exe> <label>

Everything binds 127.0.0.1 and talks to nothing else: a directory of its
own (-masterserver), a dedicated server registered with it (name, players,
one bot, a rotation file in the sandbox; both started with -console and
-debuglog and not redirected, so they attach to this script's console and
receive its Ctrl+C, and what they print is read back from their logs), the directory's listing as
-servers and -hosts print it, one scripted -netcheck client, and both
servers stopped with a real Ctrl+C -- the only signal the Windows build
listens for. A few one-shot refusals are run as well. Writes
runs/<label>-server/report.txt for compare-server-loopback.py.

The script re-runs itself inside a console of its own, because Ctrl+C on
Windows goes to every process on a console: the copy that sends it must not
share one with whoever started the test.
"""
import ctypes
import os
import pathlib
import re
import subprocess
import sys
import time

MASTER_PORT = 27991
SERVER_PORT = 27992
NAME = "ME06 Loopback"
HERE = pathlib.Path(__file__).resolve().parent


def start(exe, args, out, cwd):
    return subprocess.Popen([exe, *args], cwd=cwd, stdout=open(out.with_suffix(".out.txt"), "wb"),
                            stderr=open(out.with_suffix(".err.txt"), "wb"), stdin=subprocess.DEVNULL)


def start_attached(exe, args, cwd):
    # Not redirected: a redirected GUI build never sees this console's
    # Ctrl+C. What it prints is read back from its -debuglog file instead.
    return subprocess.Popen([exe, *args, "-console", "-debuglog"], cwd=cwd, stdin=subprocess.DEVNULL)


def collect_log(exe, process, dest):
    logs = pathlib.Path(exe).parent / "logs"
    found = sorted(logs.glob(f"FruityPrime-*-{process.pid}.log"))
    dest.write_bytes(found[-1].read_bytes() if found else b"<no log>")


def run(exe, args, out, cwd, timeout):
    p = start(exe, args, out, cwd)
    try:
        return p.wait(timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        return "TIMEOUT"


def ctrl_c(processes, timeout=30):
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleCtrlHandler(None, True)  # this script ignores it
    kernel32.GenerateConsoleCtrlEvent(0, 0)     # CTRL_C_EVENT to the console
    codes = []
    for p in processes:
        try:
            codes.append(p.wait(timeout))
        except subprocess.TimeoutExpired:
            p.kill()
            codes.append("TIMEOUT")
    return codes


def prime_console():
    # Measured on this rig: the first Ctrl+C sent in a freshly created
    # console never reaches a process started before it, and every later one
    # does -- with the baseline build as with any other. One is sent and
    # ignored before anything starts.
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleCtrlHandler(None, True)
    kernel32.GenerateConsoleCtrlEvent(0, 0)
    time.sleep(0.5)
    kernel32.SetConsoleCtrlHandler(None, False)


def scenario(exe, label):
    prime_console()
    out = HERE / "runs" / f"{label}-server"
    out.mkdir(parents=True, exist_ok=True)
    for f in out.iterdir():
        f.unlink()
    cwd = str(out)
    rotation = out / "rotation.txt"
    rotation.write_text("TEST ARENA | Battle | 7 | 7\nMP3 PROVING GROUND | Battle | 7 | 7\n", encoding="utf-8")
    common = ["-noupdate", "-noautoupdate"]
    report = []

    # One-shot refusals and notes, before anything is listening.
    # A directory refusing a bad -hostports keeps running: stopped by the
    # timeout, so the expected result is TIMEOUT and the line it printed.
    report.append(("refusal-hostports", run(exe, ["-masterserver", "-port", "27993", "-hostports", "bad", *common],
                                            out / "refusal-hostports", cwd, 4)))

    master = start_attached(exe, ["-masterserver", "-port", str(MASTER_PORT), "-hostports", "none", *common], cwd)
    time.sleep(2)
    server = start_attached(exe, ["-server", "-port", str(SERVER_PORT), "-players", "4", "-servername", NAME,
                         "-rotation", str(rotation), "-bots", "1", "-botlevel", "2", "-simulate",
                         "-master", "127.0.0.1", "-masterport", str(MASTER_PORT), *common], cwd)
    time.sleep(12)
    report.append(("servers", run(exe, ["-servers", "-master", "127.0.0.1", "-masterport", str(MASTER_PORT), *common],
                                  out / "servers", cwd, 30)))
    report.append(("hosts", run(exe, ["-hosts", "-master", "127.0.0.1", "-masterport", str(MASTER_PORT), *common],
                                out / "hosts", cwd, 40)))
    report.append(("netcheck", run(exe, ["-netcheck", "127.0.0.1", "-port", str(SERVER_PORT), "-name", "NC",
                                         "-hunter", "Samus", "-seconds", "8", "-headless", *common],
                                   out / "netcheck", cwd, 120)))
    # -connect: the real game client in a window, joined for a while and
    # then closed the hard way; what is compared is that the server saw it.
    connect = start(exe, ["-connect", "127.0.0.1", "-port", str(SERVER_PORT), "-name", "CN",
                          "-hunter", "Kanden", *common], out / "connect", cwd)
    time.sleep(12)
    report.append(("connect-running-after-12s", connect.poll() is None))
    connect.kill()
    connect.wait(30)
    time.sleep(8)
    report.append(("servers-after", run(exe, ["-servers", "-master", "127.0.0.1", "-masterport", str(MASTER_PORT), *common],
                                        out / "servers-after", cwd, 30)))
    alive = [("server-alive-before-stop", server.poll() is None), ("master-alive-before-stop", master.poll() is None)]
    server_code, master_code = ctrl_c([server, master])
    report.extend(alive)
    report.append(("server-exit-after-ctrl-c", server_code))
    report.append(("master-exit-after-ctrl-c", master_code))
    collect_log(exe, server, out / "server.log")
    collect_log(exe, master, out / "master.log")
    server_log = (out / "server.log").read_text(encoding="utf-8-sig", errors="replace")
    report.append(("connect-joined-server", '"CN" playing Kanden' in server_log))
    with open(out / "report.txt", "w", encoding="utf-8") as f:
        for key, value in report:
            f.write(f"{key}\t{value}\n")


def main():
    exe, label = sys.argv[1], sys.argv[2]
    if os.environ.get("ME06_INNER") != "1":
        env = dict(os.environ, ME06_INNER="1")
        p = subprocess.Popen([sys.executable, "-I", __file__, exe, label], env=env,
                             creationflags=subprocess.CREATE_NEW_CONSOLE)
        sys.exit(p.wait())
    scenario(exe, label)


if __name__ == "__main__":
    main()
