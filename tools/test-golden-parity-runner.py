#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest


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
                    "--exe",
                    str(self.root / "stale-FruityPrime"),
                )
            )


if __name__ == "__main__":
    unittest.main()
