#!/usr/bin/env python3
"""Android startup gate: install the APK on a running emulator/device, launch
it, and prove that what the player sees is never a black window.

    python3 tools/ci/android-startup-smoke.py APK [--out DIR] [--case NAME ...]

Cases:
  normal        Qt READY -> native_create_ok -> front_published ->
                first_qt_frame_presented, and a screenshot that is not blank.
  qml-error     an invalid QML source is injected: qml_status_error, the
                persistent failure panel, a non-blank screenshot, no ANR.
  native-fail   nativeCreate is made to throw: native_create_failed, the
                failure panel, a non-blank screenshot.

Injection needs a debuggable APK or the marker file this script pushes into
the app's external files directory (see MainActivity.startupTestHooks).
Exit code 0 = every case passed. Needs only adb on PATH (or ANDROID_SDK_ROOT).
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import time

PACKAGE = "fr.livetek.fruityprime"
ACTIVITY = PACKAGE + "/.MainActivity"
MARKER = "/sdcard/Android/data/%s/files/fruity-startup-test" % PACKAGE
PROPERTY = "debug.fruityprime.startuptest"
FATAL = ("FATAL EXCEPTION", "Fatal signal", "ANR in " + PACKAGE)


def adb_path():
    found = shutil.which("adb")
    if found:
        return found
    for root in (os.environ.get("ANDROID_SDK_ROOT"), os.environ.get("ANDROID_HOME"),
                 os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk")):
        if root:
            candidate = os.path.join(root, "platform-tools", "adb.exe" if os.name == "nt" else "adb")
            if os.path.isfile(candidate):
                return candidate
    sys.exit("adb not found")


ADB = adb_path()


def adb(*args, check=True, binary=False, timeout=120):
    result = subprocess.run([ADB, *args], capture_output=True, timeout=timeout)
    if check and result.returncode != 0:
        raise RuntimeError("adb %s failed: %s" % (" ".join(args), result.stderr.decode(errors="replace")))
    return result.stdout if binary else result.stdout.decode(errors="replace")


def logcat():
    return adb("logcat", "-d", "-v", "brief", "FruityStartup:V", "FruityQt:V", "AndroidRuntime:E",
               "ActivityManager:E", "DEBUG:V", "libc:F", "*:S")


def screenshot_variance(path):
    """Raw screencap: header (w, h, format[, colorspace]) then RGBA rows."""
    raw = adb("exec-out", "screencap", binary=True)
    width, height, _ = struct.unpack_from("<III", raw, 0)
    header = 16 if len(raw) >= 16 + width * height * 4 else 12
    pixels = raw[header:header + width * height * 4]
    with open(path, "wb") as out:
        out.write(raw)
    # Sample a grid of luminances; a blank window is one value everywhere.
    step = max(1, (width * height) // 20000)
    values = []
    for index in range(0, width * height, step):
        r, g, b = pixels[index * 4], pixels[index * 4 + 1], pixels[index * 4 + 2]
        values.append((r * 299 + g * 587 + b * 114) // 1000)
    mean = sum(values) / len(values)
    variance = sum((v - mean) ** 2 for v in values) / len(values)
    return mean, variance, len(set(values))


def run_case(name, out_dir, timeout):
    print("== case %s" % name)
    adb("shell", "am", "force-stop", PACKAGE, check=False)
    adb("logcat", "-c", check=False)
    extras = []
    if name == "qml-error":
        extras = ["--es", "fruity.testQmlSource", "qrc:/qt/qml/FruityPrime/Ui/DoesNotExist.qml"]
    elif name == "native-fail":
        extras = ["--ez", "fruity.testNativeCreateFail", "true"]
    # Two ways to arm the hooks, since scoped storage refuses the shell some
    # writes into Android/data: the marker file, and a debug.* property.
    if extras:
        local = os.path.join(out_dir, "fruity-startup-test")
        open(local, "w").close()
        adb("push", local, MARKER, check=False)
        adb("shell", "setprop", PROPERTY, "1")
    else:
        adb("shell", "rm", "-f", MARKER, check=False)
        adb("shell", "setprop", PROPERTY, "0", check=False)
    print(adb("shell", "am", "start", "-W", "-n", ACTIVITY, *extras).strip())

    expected = {
        "normal": ["qml_status_ready", "native_create_ok", "front_published", "first_qt_frame_presented"],
        "qml-error": ["qml_status_error", "failure_panel"],
        "native-fail": ["native_create_failed", "failure_panel"],
    }[name]
    deadline = time.time() + timeout
    log = ""
    while time.time() < deadline:
        log = logcat()
        if all(marker in log for marker in expected) or any(f in log for f in FATAL):
            break
        time.sleep(1)
    # Let the frame settle, then look at it.
    time.sleep(3)
    log = logcat()
    with open(os.path.join(out_dir, "%s-logcat.txt" % name), "w", encoding="utf-8") as out:
        out.write(log)
    failures = ["missing marker %s" % marker for marker in expected if marker not in log]
    failures += ["fatal: %s" % f for f in FATAL if f in log]
    pid = adb("shell", "pidof", PACKAGE, check=False).strip()
    if not pid:
        failures.append("process is not alive")
    mean, variance, levels = screenshot_variance(os.path.join(out_dir, "%s-screen.raw" % name))
    print("screen mean=%.1f variance=%.1f levels=%d" % (mean, variance, levels))
    if levels < 3 or variance < 1.0:
        failures.append("screenshot is blank (mean=%.1f variance=%.2f levels=%d)" % (mean, variance, levels))
    for line in log.splitlines():
        if "[android-startup]" in line:
            print("  " + line.strip())
    for failure in failures:
        print("FAIL %s: %s" % (name, failure))
    if not failures:
        print("PASS %s" % name)
    adb("shell", "rm", "-f", MARKER, check=False)
    adb("shell", "setprop", PROPERTY, "0", check=False)
    return not failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("apk")
    parser.add_argument("--out", default="android-startup-smoke")
    parser.add_argument("--case", action="append", choices=["normal", "qml-error", "native-fail"])
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    adb("wait-for-device", timeout=600)
    print(adb("install", "-r", "-g", args.apk, timeout=600).strip())
    results = [run_case(case, args.out, args.timeout) for case in (args.case or ["normal", "qml-error", "native-fail"])]
    adb("shell", "am", "force-stop", PACKAGE, check=False)
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
