#!/usr/bin/env python3
"""Prepare and run byte-identical Phase 3/Phase 4 golden parity captures.

The immutable revision stays at the requested Git HEAD. Only the three
GoldenCapture harness files are overlaid in the working tree, and the parity
validator independently recomputes those bytes before accepting any capture.
Validate both capture sets before using the restore command.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
from typing import Mapping, Sequence

PHASE3_BASELINE = "13c49e35f2a314662c7cc639e5e56fa784ac8bfd"
CANDIDATES = (
    "transparent-object",
    "decal",
    "particle",
    "trail",
    "hud",
    "fade",
    "whiteout-disruption",
)
SOURCE_PATHS = (
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCapture.cpp"),
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCapture.hpp"),
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCaptureValidation.hpp"),
)
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
ADAPTER_DIR = SCRIPT_DIR / "golden-parity" / "adapter"
ADAPTER_BY_SOURCE = {
    SOURCE_PATHS[0]: ADAPTER_DIR / "GoldenCapture.cpp",
    SOURCE_PATHS[1]: ADAPTER_DIR / "GoldenCapture.hpp",
    SOURCE_PATHS[2]: ADAPTER_DIR / "GoldenCaptureValidation.hpp",
}


class GateError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise GateError(message)


def run_git(root: pathlib.Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[bytes]:
    try:
        completed = subprocess.run(
            ("git", "-C", str(root), *args),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        fail(f"{root}: cannot execute git: {exc}")
    if check and completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", "replace").strip()
        fail(f"{root}: git {' '.join(args)} failed: {detail}")
    return completed


def git_text(root: pathlib.Path, *args: str) -> str:
    return run_git(root, *args).stdout.decode("utf-8", "strict").strip()


def verify_head(root: pathlib.Path, expected_commit: str) -> str:
    if not root.is_dir():
        fail(f"source root does not exist: {root}")
    head = git_text(root, "rev-parse", "--verify", "HEAD").lower()
    if head != expected_commit.lower():
        fail(f"{root}: HEAD {head} != expected {expected_commit.lower()}")
    return head


def dirty_paths(root: pathlib.Path) -> set[str]:
    dirty: set[str] = set()
    for command in (
        ("diff", "--name-only", "HEAD", "--"),
        ("diff", "--cached", "--name-only", "HEAD", "--"),
        ("ls-files", "--others", "--exclude-standard"),
    ):
        output = git_text(root, *command)
        dirty.update(
            line.replace("\\", "/")
            for line in output.splitlines()
            if line.strip()
        )
    return dirty


def adapter_bytes() -> Mapping[pathlib.Path, bytes]:
    result: dict[pathlib.Path, bytes] = {}
    for source_path, adapter_path in ADAPTER_BY_SOURCE.items():
        if not adapter_path.is_file():
            fail(f"missing tracked parity adapter: {adapter_path}")
        result[source_path] = adapter_path.read_bytes()
    return result


def verify_overlay(root: pathlib.Path, expected_commit: str) -> None:
    verify_head(root, expected_commit)
    expected = adapter_bytes()
    dirty = dirty_paths(root)
    allowed = {path.as_posix() for path in SOURCE_PATHS}
    unexpected = dirty.difference(allowed)
    if unexpected:
        fail(f"{root}: non-harness working-tree changes present: {sorted(unexpected)}")
    for relative, content in expected.items():
        target = root / relative
        if not target.is_file() or target.read_bytes() != content:
            fail(f"{root}: parity adapter is not installed at {relative.as_posix()}")
    if not dirty:
        fail(f"{root}: parity adapter unexpectedly left the source tree clean")


def prepare(root: pathlib.Path, expected_commit: str) -> None:
    verify_head(root, expected_commit)
    expected = adapter_bytes()
    allowed = {path.as_posix() for path in SOURCE_PATHS}
    dirty = dirty_paths(root)
    unexpected = dirty.difference(allowed)
    if unexpected:
        fail(f"{root}: refusing to overwrite unrelated changes: {sorted(unexpected)}")

    for relative, content in expected.items():
        target = root / relative
        if relative.as_posix() in dirty and target.exists() and target.read_bytes() != content:
            fail(
                f"{root}: refusing to replace pre-existing harness edit "
                f"{relative.as_posix()}"
            )

    for relative, content in expected.items():
        target = root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)

    verify_overlay(root, expected_commit)
    identity = harness_identity(root)
    print(f"prepared {root}")
    print(f"source_commit={expected_commit.lower()}")
    print(f"golden_capture_harness_sha256={identity['harness_sha256']}")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git_blob_sha1(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def harness_identity(root: pathlib.Path) -> dict[str, str]:
    contents = [(root / path).read_bytes() for path in SOURCE_PATHS]
    hashes = [sha256(content) for content in contents]
    return {
        "cpp_git_blob": git_blob_sha1(contents[0]),
        "cpp_sha256": hashes[0],
        "harness_sha256": sha256(":".join(hashes).encode("ascii")),
    }


def parse_manifest(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line:
            continue
        if "=" not in line:
            fail(f"{path}:{number}: malformed manifest line")
        key, value = line.split("=", 1)
        if key in result:
            fail(f"{path}:{number}: duplicate manifest key {key}")
        result[key] = value
    return result


def capture(
    root: pathlib.Path,
    expected_commit: str,
    executable: pathlib.Path,
    output: pathlib.Path,
    cwd: pathlib.Path,
    mapdir: pathlib.Path | None,
) -> None:
    verify_overlay(root, expected_commit)
    identity = harness_identity(root)

    if output.exists() and any(output.iterdir()):
        fail(
            f"capture output is not empty: {output}; "
            "fresh parity evidence must use a new/empty directory"
        )
    output.mkdir(parents=True, exist_ok=True)
    if not executable.is_file():
        fail(f"capture executable does not exist: {executable}")
    if not cwd.is_dir():
        fail(f"capture working directory does not exist: {cwd}")

    env = os.environ.copy()
    env["FRUITY_GOLDEN_PARITY_SOURCE_COMMIT"] = expected_commit.lower()
    env["FRUITY_GOLDEN_PARITY_CPP_GIT_BLOB"] = identity["cpp_git_blob"]
    env["FRUITY_GOLDEN_PARITY_CPP_SHA256"] = identity["cpp_sha256"]
    env["FRUITY_GOLDEN_PARITY_HARNESS_SHA256"] = identity["harness_sha256"]

    command = [
        str(executable.resolve()),
        "-goldencapture",
        "all",
        "-goldendir",
        str(output.resolve()),
    ]
    if mapdir is not None:
        command.extend(("-mapdir", str(mapdir.resolve())))

    completed = subprocess.run(command, cwd=str(cwd.resolve()), env=env, check=False)
    if completed.returncode != 0:
        fail(f"golden capture exited with code {completed.returncode}")

    for candidate in CANDIDATES:
        image = output / f"{candidate}.png"
        manifest_path = output / f"{candidate}.txt"
        if not image.is_file() or not manifest_path.is_file():
            fail(f"capture did not produce both files for {candidate}")
        manifest = parse_manifest(manifest_path)
        expected_fields = {
            "source_commit": expected_commit.lower(),
            "source_commit_harness_state": "dirty",
            "golden_capture_cpp_git_blob": identity["cpp_git_blob"],
            "golden_capture_cpp_sha256": identity["cpp_sha256"],
            "golden_capture_harness_sha256": identity["harness_sha256"],
            "parity_adapter_contract": "phase3-phase4-shared-v1",
            "captured": "true",
        }
        for key, expected_value in expected_fields.items():
            actual = manifest.get(key)
            if actual != expected_value:
                fail(
                    f"{manifest_path}: {key}={actual!r} "
                    f"!= expected {expected_value!r}"
                )

    print(f"captured={output.resolve()}")
    print(f"source_commit={expected_commit.lower()}")
    print(f"golden_capture_harness_sha256={identity['harness_sha256']}")
    print("Run tools/validate-golden-parity.py before restoring either overlay.")


def restore(root: pathlib.Path, expected_commit: str) -> None:
    verify_head(root, expected_commit)
    unexpected = dirty_paths(root).difference(
        {path.as_posix() for path in SOURCE_PATHS}
    )
    if unexpected:
        fail(f"{root}: refusing restore with unrelated changes: {sorted(unexpected)}")

    for relative in SOURCE_PATHS:
        spec = f"HEAD:{relative.as_posix()}"
        tracked = run_git(root, "cat-file", "-e", spec, check=False).returncode == 0
        target = root / relative
        if tracked:
            content = run_git(root, "show", spec).stdout
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        elif target.exists():
            target.unlink()

    remaining = dirty_paths(root)
    if remaining:
        fail(f"{root}: restore left working-tree changes: {sorted(remaining)}")
    print(f"restored {root} to {expected_commit.lower()}")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    for name in ("prepare", "status", "restore"):
        sub = subparsers.add_parser(name)
        sub.add_argument("--source-root", type=pathlib.Path, required=True)
        sub.add_argument("--expected-commit", required=True)

    sub = subparsers.add_parser("capture")
    sub.add_argument("--source-root", type=pathlib.Path, required=True)
    sub.add_argument("--expected-commit", required=True)
    sub.add_argument("--exe", type=pathlib.Path, required=True)
    sub.add_argument("--output", type=pathlib.Path, required=True)
    sub.add_argument("--cwd", type=pathlib.Path, default=pathlib.Path.cwd())
    sub.add_argument("--mapdir", type=pathlib.Path)
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    try:
        root = args.source_root.resolve()
        expected = args.expected_commit.lower()
        if len(expected) != 40 or any(ch not in "0123456789abcdef" for ch in expected):
            fail(f"invalid expected commit: {args.expected_commit!r}")

        if args.command == "prepare":
            prepare(root, expected)
        elif args.command == "status":
            verify_overlay(root, expected)
            identity = harness_identity(root)
            print(f"source_commit={expected}")
            print(f"golden_capture_cpp_git_blob={identity['cpp_git_blob']}")
            print(f"golden_capture_cpp_sha256={identity['cpp_sha256']}")
            print(f"golden_capture_harness_sha256={identity['harness_sha256']}")
        elif args.command == "capture":
            capture(
                root,
                expected,
                args.exe,
                args.output,
                args.cwd,
                args.mapdir,
            )
        elif args.command == "restore":
            restore(root, expected)
        else:
            fail(f"unknown command {args.command}")
        return 0
    except (GateError, OSError, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
