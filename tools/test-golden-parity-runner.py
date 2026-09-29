#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock


TOOLS_DIR = pathlib.Path(__file__).resolve().parent
RUNNER_PATH = TOOLS_DIR / "run-golden-parity-capture.py"
SPEC = importlib.util.spec_from_file_location("golden_parity_runner", RUNNER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot import {RUNNER_PATH}")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def git(root: pathlib.Path, *args: str) -> str:
    completed = subprocess.run(
        ("git", "-C", str(root), *args),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return completed.stdout.decode("utf-8", "strict").strip()


class RunnerLifecycleTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._temp.name) / "source"
        self.root.mkdir()
        subprocess.run(("git", "init", "-q", str(self.root)), check=True)
        git(self.root, "config", "user.email", "golden-parity@test.invalid")
        git(self.root, "config", "user.name", "Golden Parity Test")

        for index, relative in enumerate(runner.SOURCE_PATHS):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(f"baseline-{index}\n".encode("ascii"))
        git(self.root, "add", ".")
        git(self.root, "commit", "-q", "-m", "baseline")
        self.commit = git(self.root, "rev-parse", "HEAD")

    def tearDown(self) -> None:
        self._temp.cleanup()

    def snapshot_targets(self) -> dict[str, bytes]:
        return {
            relative.as_posix(): (self.root / relative).read_bytes()
            for relative in runner.SOURCE_PATHS
        }

    def absolute_paths_file(self) -> pathlib.Path:
        path = pathlib.Path(self._temp.name) / "runtime-paths.txt"
        path.write_text(
            "0.19.0.0\nAMHE0=C:\\golden-parity\\AMHE0\nExport=\n",
            encoding="utf-8",
        )
        return path

    def test_restore_refuses_changed_after_prepare_without_writing(self) -> None:
        runner.prepare(self.root, self.commit)
        changed = self.root / runner.SOURCE_PATHS[0]
        changed.write_bytes(b"user edit after prepare\n")
        before = self.snapshot_targets()

        with self.assertRaises(runner.GateError):
            runner.restore(self.root, self.commit)

        self.assertEqual(self.snapshot_targets(), before)

    def test_restore_refuses_staged_overlay_without_writing(self) -> None:
        runner.prepare(self.root, self.commit)
        git(self.root, "add", runner.SOURCE_PATHS[0].as_posix())
        before = self.snapshot_targets()

        with self.assertRaises(runner.GateError):
            runner.restore(self.root, self.commit)

        self.assertEqual(self.snapshot_targets(), before)
        self.assertIn(
            runner.SOURCE_PATHS[0].as_posix(),
            runner.staged_paths(self.root),
        )

    def test_prepare_refuses_preexisting_matching_adapter(self) -> None:
        expected = runner.adapter_bytes()
        target = self.root / runner.SOURCE_PATHS[0]
        target.write_bytes(expected[runner.SOURCE_PATHS[0]])
        before = self.snapshot_targets()

        with self.assertRaises(runner.GateError):
            runner.prepare(self.root, self.commit)

        self.assertEqual(self.snapshot_targets(), before)

    def test_runtime_paths_stage_is_exact_and_cleanup_is_nondestructive(self) -> None:
        source = self.absolute_paths_file()
        original = source.read_bytes()
        executable_dir = pathlib.Path(self._temp.name) / "build"
        executable_dir.mkdir()
        executable = executable_dir / "FruityPrime.exe"
        executable.write_bytes(b"binary")
        destination, digest = runner.stage_runtime_paths_file(source, executable)
        self.assertEqual(destination.read_bytes(), original)
        self.assertEqual(source.read_bytes(), original)
        runner.remove_staged_runtime_paths_file(destination, digest)
        self.assertFalse(destination.exists())
        self.assertEqual(source.read_bytes(), original)

    def test_runtime_paths_stage_rejects_relative_game_path(self) -> None:
        source = pathlib.Path(self._temp.name) / "relative-paths.txt"
        source.write_text("0.19.0.0\nAMHE0=files\\AMHE0\n", encoding="utf-8")
        executable_dir = pathlib.Path(self._temp.name) / "build-relative"
        executable_dir.mkdir()
        executable = executable_dir / "FruityPrime.exe"
        executable.write_bytes(b"binary")
        with self.assertRaises(runner.GateError):
            runner.stage_runtime_paths_file(source, executable)
        self.assertFalse((executable_dir / "paths.txt").exists())

    def test_runtime_paths_cleanup_preserves_changed_staged_copy(self) -> None:
        source = self.absolute_paths_file()
        executable_dir = pathlib.Path(self._temp.name) / "build-changed"
        executable_dir.mkdir()
        executable = executable_dir / "FruityPrime.exe"
        executable.write_bytes(b"binary")
        destination, digest = runner.stage_runtime_paths_file(source, executable)
        destination.write_bytes(b"changed after staging\n")
        with self.assertRaises(runner.GateError):
            runner.remove_staged_runtime_paths_file(destination, digest)
        self.assertEqual(destination.read_bytes(), b"changed after staging\n")

    def test_runtime_paths_stage_refuses_existing_destination(self) -> None:
        source = self.absolute_paths_file()
        executable_dir = pathlib.Path(self._temp.name) / "build-existing"
        executable_dir.mkdir()
        executable = executable_dir / "FruityPrime.exe"
        executable.write_bytes(b"binary")
        destination = executable_dir / "paths.txt"
        destination.write_bytes(b"pre-existing\n")
        with self.assertRaises(runner.GateError):
            runner.stage_runtime_paths_file(source, executable)
        self.assertEqual(destination.read_bytes(), b"pre-existing\n")

    def test_run_command_forwards_environment(self) -> None:
        environment = {"FRUITY_GOLDEN_PARITY_SOURCE_COMMIT": "sentinel"}
        completed = mock.Mock()
        completed.returncode = 0
        with mock.patch.object(runner.subprocess, "run", return_value=completed) as run:
            runner.run_command(
                ("fake-command",),
                cwd=self.root,
                description="environment forwarding test",
                env=environment,
            )
        self.assertIs(run.call_args.kwargs["env"], environment)

    def test_build_provenance_rejects_binary_source_mismatch(self) -> None:
        build_dir = self.root / "tools" / "build" / "out" / "golden-parity" / "test"
        build_dir.mkdir(parents=True)
        executable = build_dir / "FruityPrime"
        executable.write_bytes(b"not a real executable")
        identity = {"harness_sha256": "a" * 64}
        payload = {
            "source_root": str(self.root.resolve()),
            "source_commit": "0" * 40,
            "harness_sha256": identity["harness_sha256"],
            "executable": "FruityPrime",
            "executable_sha256": runner.sha256_file(executable),
        }
        runner.build_provenance_path(build_dir).write_text(
            json.dumps(payload),
            encoding="utf-8",
        )

        with self.assertRaises(runner.GateError):
            runner.verify_build_provenance(
                self.root,
                self.commit,
                identity,
                build_dir,
                executable,
            )

    def test_capture_cli_does_not_accept_caller_supplied_executable(self) -> None:
        with self.assertRaises(SystemExit):
            runner.parse_args(
                (
                    "capture",
                    "--source-root",
                    str(self.root),
                    "--expected-commit",
                    self.commit,
                    "--output",
                    str(self.root / "capture"),
                    "--paths-file",
                    str(self.absolute_paths_file()),
                    "--mapdir",
                    str(self.root),
                    "--exe",
                    str(self.root / "stale-FruityPrime"),
                )
            )


if __name__ == "__main__":
    unittest.main()
