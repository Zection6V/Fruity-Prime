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
import json
import ntpath
import os
import pathlib
import re
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

GAME_PATH_KEYS = frozenset((
    "AMFE0", "AMFP0", "A76E0", "AMHE0", "AMHE1",
    "AMHP0", "AMHP1", "AMHJ0", "AMHJ1", "AMHK0",
))
NON_GAME_PATH_KEYS = frozenset(("Export",))
RUNTIME_PROVENANCE_NAME = "golden-parity-runtime.json"
STARTUP_MAPS_DIRECTORY_NAME = "maps"


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


def staged_paths(root: pathlib.Path) -> set[str]:
    output = git_text(root, "diff", "--cached", "--name-only", "HEAD", "--")
    return {
        line.replace("\\", "/")
        for line in output.splitlines()
        if line.strip()
    }


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

    staged = staged_paths(root).intersection(allowed)
    if staged:
        fail(f"{root}: parity harness has staged changes: {sorted(staged)}")

    for relative, content in expected.items():
        target = root / relative
        if not target.is_file() or target.read_bytes() != content:
            fail(f"{root}: parity adapter is not installed at {relative.as_posix()}")

    if not dirty.intersection(allowed):
        fail(f"{root}: parity adapter unexpectedly left the source tree clean")


def prepare(root: pathlib.Path, expected_commit: str) -> None:
    verify_head(root, expected_commit)
    allowed = {path.as_posix() for path in SOURCE_PATHS}
    dirty = dirty_paths(root)
    unexpected = dirty.difference(allowed)
    if unexpected:
        fail(f"{root}: refusing to overwrite unrelated changes: {sorted(unexpected)}")

    pre_existing = dirty.intersection(allowed)
    if pre_existing:
        fail(
            f"{root}: refusing to overlay a pre-existing harness change: "
            f"{sorted(pre_existing)}"
        )

    expected = adapter_bytes()
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


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _decode_paths_file(data: bytes) -> str:
    if data.startswith(b"\xff\xfe\x00\x00") or data.startswith(b"\x00\x00\xfe\xff"):
        encoding = "utf-32"
    elif data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        encoding = "utf-16"
    elif data.startswith(b"\xef\xbb\xbf"):
        encoding = "utf-8-sig"
    else:
        encoding = "utf-8"
    try:
        return data.decode(encoding, errors="strict")
    except UnicodeError as exc:
        fail(f"paths.txt cannot be decoded safely: {exc}")


def validate_runtime_paths_file(path: pathlib.Path) -> bytes:
    if not path.is_file():
        fail(f"runtime paths file does not exist: {path}")
    data = path.read_bytes()
    lines = _decode_paths_file(data).splitlines()
    if not lines or not lines[0].strip():
        fail("runtime paths file has no version line")

    seen: set[str] = set()
    for line_number, line in enumerate(lines[1:], 2):
        if not line:
            continue
        if "=" not in line:
            fail(f"runtime paths file line {line_number} is malformed")
        key, value = line.split("=", 1)
        if key not in GAME_PATH_KEYS and key not in NON_GAME_PATH_KEYS:
            fail(f"runtime paths file line {line_number} has unsupported key {key!r}")
        if key in seen:
            fail(f"runtime paths file contains duplicate key {key!r}")
        seen.add(key)
        if key in GAME_PATH_KEYS and value and not ntpath.isabs(value):
            fail(
                f"runtime paths file key {key!r} is relative; parity capture "
                "refuses to rebase or rewrite paths.txt"
            )

    if not any(key in seen for key in GAME_PATH_KEYS):
        fail("runtime paths file contains no game path entries")
    return data


def stage_runtime_paths_file(source: pathlib.Path, executable: pathlib.Path) -> tuple[pathlib.Path, str]:
    source = source.resolve()
    data = validate_runtime_paths_file(source)
    destination = executable.parent / "paths.txt"
    if destination.exists():
        fail(
            f"{destination}: refusing to overwrite a pre-existing runtime "
            "paths file in the verified build output"
        )
    digest = sha256(data)
    try:
        with destination.open("xb") as stream:
            stream.write(data)
    except OSError as exc:
        fail(f"could not stage runtime paths file: {exc}")
    if destination.read_bytes() != data:
        fail("staged runtime paths file does not match its source bytes")
    return destination, digest


def remove_staged_runtime_paths_file(destination: pathlib.Path, expected_sha256: str) -> None:
    if not destination.is_file():
        fail("staged runtime paths file disappeared; refusing destructive cleanup")
    if sha256_file(destination) != expected_sha256:
        fail(
            "staged runtime paths file changed during capture; preserving it "
            "instead of deleting it"
        )
    destination.unlink()


def _tree_relative_bytes(path: pathlib.Path) -> bytes:
    return path.as_posix().encode("utf-8", "surrogatepass")


def fixture_tree_identity(source: pathlib.Path) -> str:
    source = source.resolve()
    digest = hashlib.sha256()
    file_count = 0

    if source.is_file():
        if source.is_symlink():
            fail("startup map fixture may not be a symbolic link")
        digest.update(b"F\0")
        digest.update(_tree_relative_bytes(pathlib.Path(source.name)))
        digest.update(b"\0")
        digest.update(sha256_file(source).encode("ascii"))
        digest.update(b"\n")
        return digest.hexdigest()

    if not source.is_dir():
        fail("startup map fixture does not exist or is not a file/directory")

    entries = sorted(
        source.rglob("*"),
        key=lambda path: path.relative_to(source).as_posix(),
    )
    for entry in entries:
        if entry.is_symlink():
            fail("startup map fixture tree may not contain symbolic links")
        relative = entry.relative_to(source)
        if entry.is_dir():
            digest.update(b"D\0")
            digest.update(_tree_relative_bytes(relative))
            digest.update(b"\n")
        elif entry.is_file():
            file_count += 1
            digest.update(b"F\0")
            digest.update(_tree_relative_bytes(relative))
            digest.update(b"\0")
            digest.update(sha256_file(entry).encode("ascii"))
            digest.update(b"\n")
        else:
            fail("startup map fixture contains an unsupported filesystem entry")

    if file_count == 0:
        fail("startup map fixture directory contains no files")
    return digest.hexdigest()


def stage_startup_map_fixture(
    source: pathlib.Path,
    executable: pathlib.Path,
) -> tuple[pathlib.Path, str]:
    source = source.resolve()
    expected_before = fixture_tree_identity(source)
    destination = executable.parent / STARTUP_MAPS_DIRECTORY_NAME
    if destination.exists() or destination.is_symlink():
        fail(
            "refusing to overwrite a pre-existing maps path beside the "
            "runner-built executable"
        )

    try:
        if source.is_dir():
            shutil.copytree(source, destination, copy_function=shutil.copy2)
        else:
            destination.mkdir()
            shutil.copy2(source, destination / source.name)
    except OSError as exc:
        fail(
            "could not stage startup map fixture; preserving any partial "
            f"runner-created maps content: {exc}"
        )

    actual = fixture_tree_identity(destination)
    expected_after = fixture_tree_identity(source)
    if actual != expected_before or expected_after != expected_before:
        fail(
            "startup map fixture changed or copied inconsistently; preserving "
            "the staged maps directory instead of deleting it"
        )
    return destination, actual


def remove_staged_startup_map_fixture(
    destination: pathlib.Path,
    expected_tree_sha256: str,
) -> None:
    if not destination.is_dir() or destination.is_symlink():
        fail(
            "staged startup maps directory disappeared or changed type; "
            "refusing destructive cleanup"
        )
    if fixture_tree_identity(destination) != expected_tree_sha256:
        fail(
            "staged startup maps content changed during capture; preserving it "
            "instead of deleting it"
        )
    shutil.rmtree(destination)


def cleanup_runtime_staging(
    staged_paths: pathlib.Path | None,
    paths_sha256: str | None,
    staged_maps: pathlib.Path | None,
    maps_sha256: str | None,
) -> None:
    errors: list[str] = []
    if staged_paths is not None and paths_sha256 is not None:
        try:
            remove_staged_runtime_paths_file(staged_paths, paths_sha256)
        except GateError as exc:
            errors.append(str(exc))
    if staged_maps is not None and maps_sha256 is not None:
        try:
            remove_staged_startup_map_fixture(staged_maps, maps_sha256)
        except GateError as exc:
            errors.append(str(exc))
    if errors:
        fail("; ".join(errors))


def _path_identity(path: pathlib.Path) -> str:
    canonical = os.path.normcase(str(path.resolve()))
    return sha256(canonical.encode("utf-8"))


def write_runtime_provenance(
    output: pathlib.Path,
    expected_commit: str,
    identity: Mapping[str, str],
    paths_sha256: str,
    startup_maps_tree_sha256: str,
    mapdir: pathlib.Path,
    cwd: pathlib.Path,
    executable: pathlib.Path,
) -> None:
    payload = {
        "source_commit": expected_commit.lower(),
        "harness_sha256": identity["harness_sha256"],
        "paths_file_sha256": paths_sha256,
        "startup_maps_tree_sha256": startup_maps_tree_sha256,
        "mapdir_path_sha256": _path_identity(mapdir),
        "launch_cwd_path_sha256": _path_identity(cwd),
        "executable_sha256": sha256_file(executable),
    }
    path = output / RUNTIME_PROVENANCE_NAME
    if path.exists():
        fail(f"{path}: refusing to overwrite runtime provenance")
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_command(
    command: Sequence[str],
    *,
    cwd: pathlib.Path,
    description: str,
    env: Mapping[str, str] | None = None,
) -> None:
    try:
        completed = subprocess.run(
            tuple(command),
            cwd=str(cwd),
            env=env,
            check=False,
        )
    except OSError as exc:
        fail(f"{description} could not start: {exc}")
    if completed.returncode != 0:
        fail(f"{description} exited with code {completed.returncode}")


def resolve_build_dir(
    root: pathlib.Path,
    expected_commit: str,
    identity: Mapping[str, str],
    requested: pathlib.Path | None,
) -> pathlib.Path:
    build_base = (root / "tools" / "build" / "out" / "golden-parity").resolve()
    if requested is None:
        build_dir = (
            build_base
            / f"{expected_commit[:12]}-{identity['harness_sha256'][:12]}"
        ).resolve()
    else:
        candidate = requested if requested.is_absolute() else root / requested
        build_dir = candidate.resolve()

    try:
        relative = build_dir.relative_to(build_base)
    except ValueError:
        fail(
            f"{build_dir}: golden parity build directory must be below "
            f"{build_base}"
        )
    if not relative.parts:
        fail(f"{build_dir}: build directory must be a child of {build_base}")

    if build_dir.exists():
        if not build_dir.is_dir():
            fail(f"{build_dir}: build path exists but is not a directory")
        if any(build_dir.iterdir()):
            fail(
                f"{build_dir}: build directory is not empty; parity capture "
                "requires a fresh build tree"
            )
    else:
        build_dir.mkdir(parents=True)

    relative_to_root = build_dir.relative_to(root).as_posix()
    ignored = run_git(
        root,
        "check-ignore",
        "-q",
        "--",
        relative_to_root,
        check=False,
    )
    if ignored.returncode != 0:
        fail(f"{build_dir}: build directory is not ignored by the source tree")
    return build_dir


def parse_cmake_home(cache_path: pathlib.Path) -> pathlib.Path:
    if not cache_path.is_file():
        fail(f"{cache_path}: CMake configure did not create CMakeCache.txt")
    prefix = "CMAKE_HOME_DIRECTORY:INTERNAL="
    for line in cache_path.read_text(encoding="utf-8", errors="strict").splitlines():
        if line.startswith(prefix):
            return pathlib.Path(line[len(prefix):]).resolve()
    fail(f"{cache_path}: CMAKE_HOME_DIRECTORY is missing")


def validate_cmake_defines(defines: Sequence[str]) -> list[str]:
    result: list[str] = []
    for define in defines:
        if "=" not in define:
            fail(f"invalid --define {define!r}; expected NAME=VALUE")
        name, _value = define.split("=", 1)
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) is None:
            fail(f"invalid CMake variable name in --define {define!r}")
        if name in {"CMAKE_HOME_DIRECTORY", "CMAKE_SOURCE_DIR", "PROJECT_SOURCE_DIR"}:
            fail(f"--define may not override source provenance: {name}")
        result.append(f"-D{define}")
    return result


def discover_built_executable(build_dir: pathlib.Path) -> pathlib.Path:
    candidates = sorted(
        path.resolve()
        for path in build_dir.rglob("*")
        if path.is_file() and path.name in {"FruityPrime", "FruityPrime.exe"}
    )
    if len(candidates) != 1:
        fail(
            f"{build_dir}: expected exactly one freshly built FruityPrime "
            f"executable, found {[str(path) for path in candidates]}"
        )
    executable = candidates[0]
    try:
        executable.relative_to(build_dir)
    except ValueError:
        fail(f"{executable}: executable escaped the verified build directory")
    return executable


def build_provenance_path(build_dir: pathlib.Path) -> pathlib.Path:
    return build_dir / "golden-parity-build.json"


def write_build_provenance(
    root: pathlib.Path,
    expected_commit: str,
    identity: Mapping[str, str],
    build_dir: pathlib.Path,
    executable: pathlib.Path,
) -> None:
    payload = {
        "source_root": str(root.resolve()),
        "source_commit": expected_commit.lower(),
        "harness_sha256": identity["harness_sha256"],
        "executable": executable.relative_to(build_dir).as_posix(),
        "executable_sha256": sha256_file(executable),
    }
    build_provenance_path(build_dir).write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def verify_build_provenance(
    root: pathlib.Path,
    expected_commit: str,
    identity: Mapping[str, str],
    build_dir: pathlib.Path,
    executable: pathlib.Path,
) -> None:
    stamp = build_provenance_path(build_dir)
    if not stamp.is_file():
        fail(f"{stamp}: missing runner-generated build provenance")
    try:
        payload = json.loads(stamp.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeError) as exc:
        fail(f"{stamp}: invalid build provenance: {exc}")

    expected_fields = {
        "source_root": str(root.resolve()),
        "source_commit": expected_commit.lower(),
        "harness_sha256": identity["harness_sha256"],
        "executable": executable.relative_to(build_dir).as_posix(),
        "executable_sha256": sha256_file(executable),
    }
    for key, expected_value in expected_fields.items():
        actual = payload.get(key)
        if actual != expected_value:
            fail(
                f"{stamp}: {key}={actual!r} != expected {expected_value!r}"
            )


def build_capture_executable(
    root: pathlib.Path,
    expected_commit: str,
    *,
    requested_build_dir: pathlib.Path | None,
    config: str,
    generator: str | None,
    architecture: str | None,
    toolset: str | None,
    defines: Sequence[str],
) -> tuple[pathlib.Path, pathlib.Path, Mapping[str, str]]:
    verify_overlay(root, expected_commit)
    identity = harness_identity(root)
    build_dir = resolve_build_dir(
        root,
        expected_commit,
        identity,
        requested_build_dir,
    )

    cmake = shutil.which("cmake")
    if cmake is None:
        fail("cmake was not found on PATH")

    configure = [
        cmake,
        "-S",
        str(root),
        "-B",
        str(build_dir),
        f"-DCMAKE_BUILD_TYPE={config}",
    ]
    if generator is not None:
        configure.extend(("-G", generator))
    if architecture is not None:
        configure.extend(("-A", architecture))
    if toolset is not None:
        configure.extend(("-T", toolset))
    configure.extend(validate_cmake_defines(defines))

    run_command(
        configure,
        cwd=root,
        description="golden parity CMake configure",
    )

    configured_source = parse_cmake_home(build_dir / "CMakeCache.txt")
    if configured_source != root.resolve():
        fail(
            f"{build_dir}: CMake source {configured_source} "
            f"!= verified source {root.resolve()}"
        )

    run_command(
        (
            cmake,
            "--build",
            str(build_dir),
            "--config",
            config,
            "--target",
            "fruity_prime",
        ),
        cwd=root,
        description="golden parity fruity_prime build",
    )

    verify_overlay(root, expected_commit)
    executable = discover_built_executable(build_dir)
    write_build_provenance(
        root,
        expected_commit,
        identity,
        build_dir,
        executable,
    )
    verify_build_provenance(
        root,
        expected_commit,
        identity,
        build_dir,
        executable,
    )
    return executable, build_dir, identity


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
    output: pathlib.Path,
    cwd: pathlib.Path,
    paths_file: pathlib.Path,
    mapdir: pathlib.Path,
    startup_map_fixture: pathlib.Path | None,
    *,
    requested_build_dir: pathlib.Path | None,
    config: str,
    generator: str | None,
    architecture: str | None,
    toolset: str | None,
    defines: Sequence[str],
) -> None:
    verify_overlay(root, expected_commit)

    if output.exists() and any(output.iterdir()):
        fail(
            f"capture output is not empty: {output}; "
            "fresh parity evidence must use a new/empty directory"
        )
    cwd = cwd.resolve()
    paths_file = paths_file.resolve()
    mapdir = mapdir.resolve()
    startup_map_fixture = (
        mapdir if startup_map_fixture is None else startup_map_fixture.resolve()
    )
    if not cwd.is_dir():
        fail(f"capture working directory does not exist: {cwd}")
    if not mapdir.is_dir():
        fail(f"capture map directory does not exist: {mapdir}")
    validate_runtime_paths_file(paths_file)
    # Preflight identity without recording or printing the raw local path.
    fixture_tree_identity(startup_map_fixture)

    executable, build_dir, identity = build_capture_executable(
        root,
        expected_commit,
        requested_build_dir=requested_build_dir,
        config=config,
        generator=generator,
        architecture=architecture,
        toolset=toolset,
        defines=defines,
    )

    verify_overlay(root, expected_commit)
    verify_build_provenance(
        root,
        expected_commit,
        identity,
        build_dir,
        executable,
    )

    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["FRUITY_GOLDEN_PARITY_SOURCE_COMMIT"] = expected_commit.lower()
    env["FRUITY_GOLDEN_PARITY_CPP_GIT_BLOB"] = identity["cpp_git_blob"]
    env["FRUITY_GOLDEN_PARITY_CPP_SHA256"] = identity["cpp_sha256"]
    env["FRUITY_GOLDEN_PARITY_HARNESS_SHA256"] = identity["harness_sha256"]

    command = [
        str(executable),
        "-goldencapture",
        "all",
        "-goldendir",
        str(output.resolve()),
        "-mapdir",
        str(mapdir),
    ]

    staged_paths: pathlib.Path | None = None
    paths_sha256: str | None = None
    staged_maps: pathlib.Path | None = None
    startup_maps_tree_sha256: str | None = None
    try:
        staged_maps, startup_maps_tree_sha256 = stage_startup_map_fixture(
            startup_map_fixture,
            executable,
        )
        staged_paths, paths_sha256 = stage_runtime_paths_file(
            paths_file,
            executable,
        )
        run_command(
            command,
            cwd=cwd,
            description="golden parity capture",
            env=env,
        )
    finally:
        cleanup_runtime_staging(
            staged_paths,
            paths_sha256,
            staged_maps,
            startup_maps_tree_sha256,
        )

    if paths_sha256 is None or startup_maps_tree_sha256 is None:
        fail("runtime staging completed without provenance identities")

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

    write_runtime_provenance(
        output,
        expected_commit,
        identity,
        paths_sha256,
        startup_maps_tree_sha256,
        mapdir,
        cwd,
        executable,
    )

    print(f"captured={output.resolve()}")
    print(f"source_commit={expected_commit.lower()}")
    print(f"built_executable={executable}")
    print(f"built_executable_sha256={sha256_file(executable)}")
    print(f"golden_capture_harness_sha256={identity['harness_sha256']}")
    print("Run tools/validate-golden-parity.py before restoring either overlay.")

def restore(root: pathlib.Path, expected_commit: str) -> None:
    verify_head(root, expected_commit)
    expected = adapter_bytes()
    allowed = {path.as_posix() for path in SOURCE_PATHS}

    dirty = dirty_paths(root)
    unexpected = dirty.difference(allowed)
    if unexpected:
        fail(f"{root}: refusing restore with unrelated changes: {sorted(unexpected)}")

    staged = staged_paths(root).intersection(allowed)
    if staged:
        fail(
            f"{root}: refusing restore because harness changes are staged: "
            f"{sorted(staged)}"
        )

    mismatched: list[str] = []
    for relative, content in expected.items():
        target = root / relative
        if not target.is_file() or target.read_bytes() != content:
            mismatched.append(relative.as_posix())
    if mismatched:
        fail(
            f"{root}: refusing restore because overlay bytes changed after "
            f"prepare: {mismatched}"
        )

    plan: list[tuple[pathlib.Path, bytes | None]] = []
    for relative in SOURCE_PATHS:
        spec = f"HEAD:{relative.as_posix()}"
        tracked = run_git(root, "cat-file", "-e", spec, check=False).returncode == 0
        if tracked:
            plan.append((root / relative, run_git(root, "show", spec).stdout))
        else:
            plan.append((root / relative, None))

    staged = staged_paths(root).intersection(allowed)
    if staged:
        fail(
            f"{root}: refusing restore because harness changes became staged: "
            f"{sorted(staged)}"
        )
    for relative, content in expected.items():
        target = root / relative
        if not target.is_file() or target.read_bytes() != content:
            fail(
                f"{root}: refusing restore because overlay bytes changed "
                f"during preflight: {relative.as_posix()}"
            )

    for target, content in plan:
        if content is None:
            target.unlink()
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

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
    sub.add_argument("--output", type=pathlib.Path, required=True)
    sub.add_argument("--cwd", type=pathlib.Path, default=pathlib.Path.cwd())
    sub.add_argument(
        "--paths-file",
        type=pathlib.Path,
        required=True,
        help=(
            "local paths.txt staged unchanged beside the runner-built executable; "
            "non-empty game paths must be absolute"
        ),
    )
    sub.add_argument(
        "--mapdir",
        type=pathlib.Path,
        required=True,
        help="shared local custom-map directory used by both parity captures",
    )
    sub.add_argument(
        "--startup-map-fixture",
        type=pathlib.Path,
        help=(
            "custom-map fixture file or directory staged as executable/maps "
            "before process startup; defaults to --mapdir"
        ),
    )
    sub.add_argument(
        "--build-dir",
        type=pathlib.Path,
        help=(
            "fresh/empty build directory below "
            "<source-root>/tools/build/out/golden-parity"
        ),
    )
    sub.add_argument("--config", default="RelWithDebInfo")
    sub.add_argument("--generator")
    sub.add_argument("--architecture")
    sub.add_argument("--toolset")
    sub.add_argument(
        "--define",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="additional CMake cache definition; repeat as needed",
    )
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
                args.output,
                args.cwd,
                args.paths_file,
                args.mapdir,
                args.startup_map_fixture,
                requested_build_dir=args.build_dir,
                config=args.config,
                generator=args.generator,
                architecture=args.architecture,
                toolset=args.toolset,
                defines=args.define,
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
