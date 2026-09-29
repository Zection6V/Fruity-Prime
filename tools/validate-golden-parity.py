#!/usr/bin/env python3
"""Fail-closed Phase 3 vs Phase 4 GoldenCapture parity validator.

This tool does not create captures. It validates that two already-produced
capture sets are admissible parity evidence before comparing their decoded
pixels. In particular, it verifies the source checkouts, recomputes the
GoldenCapture composite harness hash from the actual source bytes, validates
the manifest fixture gates, and then requires exact RGB equality.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from dataclasses import dataclass
from typing import Mapping, Sequence

PHASE3_BASELINE = "13c49e35f2a314662c7cc639e5e56fa784ac8bfd"
PHASE_PLAN_BLOB = "a262838984ef547ebd6f22d1d9e78f3e1f214586"
FIXTURE_CONTRACT = "phase4-final-stage-v2"
PARITY_ADAPTER_CONTRACT = "phase3-phase4-shared-v1"
HARNESS_RELATIVE_PATHS = (
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCapture.cpp"),
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCapture.hpp"),
    pathlib.Path("src/MphRead.Native/Mods/Render/GoldenCaptureValidation.hpp"),
)
ALLOWED_HARNESS_OVERLAY_PATHS = frozenset(path.as_posix() for path in HARNESS_RELATIVE_PATHS)

CANDIDATES = (
    "transparent-object",
    "decal",
    "particle",
    "trail",
    "hud",
    "fade",
    "whiteout-disruption",
)
FINAL_STAGE_CANDIDATES = frozenset(("hud", "fade", "whiteout-disruption"))
HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")

COMMON_INPUT_KEYS = (
    "phase",
    "candidate",
    "fixture_contract",
    "parity_adapter_contract",
    "plan_baseline",
    "phase_plan_blob",
    "phase3_baseline_commit",
    "map",
    "mode",
    "hunter",
    "recolor",
    "required_dimensions",
    "actual_framebuffer",
    "actual_client",
    "actual_scene",
    "warmup_updates",
    "capture_update_ordinal",
    "trigger",
    "camera_mode",
    "camera_position",
    "camera_target",
    "fov_degrees",
    "synthetic_fixture_space",
    "resolution_scale",
    "lighting",
    "cel",
    "cel_bands",
    "cel_edge",
    "fog",
    "texture_filtering",
    "gl_context",
    "fixture",
    "fixture_scope",
    "final_stage_gate",
    "control",
)

EXPECTED_FIXED_INPUTS = {
    "phase": "4",
    "fixture_contract": FIXTURE_CONTRACT,
    "parity_adapter_contract": PARITY_ADAPTER_CONTRACT,
    "phase_plan_blob": PHASE_PLAN_BLOB,
    "phase3_baseline_commit": PHASE3_BASELINE,
    "map": "TEST ARENA",
    "mode": "Battle",
    "hunter": "Samus",
    "recolor": "0",
    "required_dimensions": "1600x900",
    "warmup_updates": "12",
    "capture_update_ordinal": "13",
    "resolution_scale": "100",
    "lighting": "on",
    "cel": "off",
    "cel_bands": "8",
    "cel_edge": "0.5",
    "fog": "off",
    "texture_filtering": "off",
}


class GateError(RuntimeError):
    pass


@dataclass(frozen=True)
class HarnessIdentity:
    cpp_sha256: str
    header_sha256: str
    validation_sha256: str
    composite_sha256: str
    cpp_git_blob: str


@dataclass(frozen=True)
class DecodedPng:
    width: int
    height: int
    rgb: bytes


@dataclass(frozen=True)
class CaptureEvidence:
    candidate: str
    manifest_path: pathlib.Path
    image_path: pathlib.Path
    manifest: Mapping[str, str]
    png: DecodedPng
    image_sha256: str


def fail(message: str) -> None:
    raise GateError(message)


def _run_git(root: pathlib.Path, *args: str) -> str:
    try:
        completed = subprocess.run(
            ("git", "-C", str(root), *args),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
    except OSError as exc:
        fail(f"{root}: cannot execute git: {exc}")
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        fail(f"{root}: git {' '.join(args)} failed: {detail}")
    return completed.stdout.strip()


def check_source_checkout(
    root: pathlib.Path,
    expected_commit: str,
    *,
    allow_harness_overlay: bool,
) -> None:
    if not root.is_dir():
        fail(f"source root does not exist: {root}")
    head = _run_git(root, "rev-parse", "--verify", "HEAD").lower()
    if head != expected_commit:
        fail(f"{root}: HEAD {head} != expected {expected_commit}")

    dirty: set[str] = set()
    for command in (
        ("diff", "--name-only", "HEAD", "--"),
        ("diff", "--cached", "--name-only", "HEAD", "--"),
        ("ls-files", "--others", "--exclude-standard"),
    ):
        output = _run_git(root, *command)
        dirty.update(line.replace("\\", "/") for line in output.splitlines() if line.strip())

    if not dirty:
        return
    if not allow_harness_overlay:
        fail(f"{root}: source checkout is dirty: {sorted(dirty)}")
    unexpected = dirty.difference(ALLOWED_HARNESS_OVERLAY_PATHS)
    if unexpected:
        fail(
            f"{root}: non-harness source changes are present: "
            f"{sorted(unexpected)}"
        )


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _git_blob_sha1(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def compute_harness_identity(root: pathlib.Path) -> HarnessIdentity:
    blobs: list[bytes] = []
    for relative in HARNESS_RELATIVE_PATHS:
        path = root / relative
        if not path.is_file():
            fail(f"{root}: missing harness source {relative.as_posix()}")
        blobs.append(path.read_bytes())

    hashes = tuple(_sha256_bytes(data) for data in blobs)
    composite = _sha256_bytes(":".join(hashes).encode("ascii"))
    return HarnessIdentity(
        cpp_sha256=hashes[0],
        header_sha256=hashes[1],
        validation_sha256=hashes[2],
        composite_sha256=composite,
        cpp_git_blob=_git_blob_sha1(blobs[0]),
    )


def parse_manifest(path: pathlib.Path) -> dict[str, str]:
    if not path.is_file():
        fail(f"missing manifest: {path}")
    result: dict[str, str] = {}
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        fail(f"cannot read manifest {path}: {exc}")
    for number, raw_line in enumerate(text.splitlines(), 1):
        if not raw_line:
            continue
        if "=" not in raw_line:
            fail(f"{path}:{number}: malformed manifest line")
        key, value = raw_line.split("=", 1)
        if not key:
            fail(f"{path}:{number}: empty manifest key")
        if key in result:
            fail(f"{path}:{number}: duplicate manifest key {key!r}")
        result[key] = value
    return result


def require_key(manifest: Mapping[str, str], key: str, path: pathlib.Path) -> str:
    if key not in manifest:
        fail(f"{path}: missing required manifest key {key!r}")
    return manifest[key]


def parse_positive_int(value: str, label: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError:
        fail(f"{label}: expected integer, got {value!r}")
    if parsed <= 0:
        fail(f"{label}: expected positive integer, got {parsed}")
    return parsed


def parse_dimensions(value: str, label: str) -> tuple[int, int]:
    match = re.fullmatch(r"([1-9][0-9]*)x([1-9][0-9]*)", value)
    if not match:
        fail(f"{label}: malformed dimensions {value!r}")
    return int(match.group(1)), int(match.group(2))


def _paeth(left: int, up: int, up_left: int) -> int:
    p = left + up - up_left
    pa = abs(p - left)
    pb = abs(p - up)
    pc = abs(p - up_left)
    if pa <= pb and pa <= pc:
        return left
    if pb <= pc:
        return up
    return up_left


def decode_png_rgb(path: pathlib.Path) -> DecodedPng:
    try:
        data = path.read_bytes()
    except OSError as exc:
        fail(f"cannot read PNG {path}: {exc}")
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"{path}: invalid PNG signature")

    offset = 8
    ihdr: bytes | None = None
    idat = bytearray()
    saw_iend = False
    while offset < len(data):
        if offset + 12 > len(data):
            fail(f"{path}: truncated PNG chunk")
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload_start = offset + 8
        payload_end = payload_start + length
        crc_end = payload_end + 4
        if crc_end > len(data):
            fail(f"{path}: truncated PNG payload")
        payload = data[payload_start:payload_end]
        expected_crc = struct.unpack(">I", data[payload_end:crc_end])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(payload, actual_crc) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            fail(f"{path}: PNG CRC mismatch in {chunk_type!r}")
        if chunk_type == b"IHDR":
            if ihdr is not None:
                fail(f"{path}: duplicate IHDR")
            ihdr = payload
        elif chunk_type == b"IDAT":
            idat.extend(payload)
        elif chunk_type == b"IEND":
            saw_iend = True
            if length != 0:
                fail(f"{path}: malformed IEND")
            break
        offset = crc_end

    if ihdr is None or len(ihdr) != 13 or not idat or not saw_iend:
        fail(f"{path}: incomplete PNG")
    width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
        ">IIBBBBB", ihdr
    )
    if width <= 0 or height <= 0:
        fail(f"{path}: invalid PNG dimensions")
    if bit_depth != 8 or compression != 0 or filter_method != 0 or interlace != 0:
        fail(
            f"{path}: unsupported PNG encoding "
            f"(bit_depth={bit_depth}, compression={compression}, "
            f"filter={filter_method}, interlace={interlace})"
        )

    channels_by_type = {0: 1, 2: 3, 4: 2, 6: 4}
    channels = channels_by_type.get(color_type)
    if channels is None:
        fail(f"{path}: unsupported PNG color type {color_type}")

    row_bytes = width * channels
    expected_raw = height * (row_bytes + 1)
    try:
        raw = zlib.decompress(bytes(idat))
    except zlib.error as exc:
        fail(f"{path}: invalid PNG deflate stream: {exc}")
    if len(raw) != expected_raw:
        fail(
            f"{path}: decoded PNG byte count {len(raw)} "
            f"!= expected {expected_raw}"
        )

    rows = bytearray(height * row_bytes)
    previous = bytearray(row_bytes)
    source_offset = 0
    for y in range(height):
        filter_type = raw[source_offset]
        source_offset += 1
        encoded = raw[source_offset : source_offset + row_bytes]
        source_offset += row_bytes
        current = bytearray(row_bytes)
        for x, value in enumerate(encoded):
            left = current[x - channels] if x >= channels else 0
            up = previous[x]
            up_left = previous[x - channels] if x >= channels else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                predictor = _paeth(left, up, up_left)
            else:
                fail(f"{path}: invalid PNG filter type {filter_type}")
            current[x] = (value + predictor) & 0xFF
        rows[y * row_bytes : (y + 1) * row_bytes] = current
        previous = current

    if color_type == 2:
        rgb = bytes(rows)
    else:
        rgb_out = bytearray(width * height * 3)
        out = 0
        for i in range(0, len(rows), channels):
            if color_type in (0, 4):
                red = green = blue = rows[i]
            else:
                red, green, blue = rows[i], rows[i + 1], rows[i + 2]
            rgb_out[out : out + 3] = bytes((red, green, blue))
            out += 3
        rgb = bytes(rgb_out)

    return DecodedPng(width=width, height=height, rgb=rgb)


def _lit_pixels(rgb: bytes) -> int:
    if len(rgb) % 3 != 0:
        fail("internal RGB byte count is not divisible by 3")
    return sum(
        1
        for i in range(0, len(rgb), 3)
        if rgb[i] > 8 or rgb[i + 1] > 8 or rgb[i + 2] > 8
    )


def validate_manifest(
    candidate: str,
    manifest: Mapping[str, str],
    manifest_path: pathlib.Path,
    image_path: pathlib.Path,
    expected_source_commit: str,
    harness: HarnessIdentity,
) -> None:
    if not HEX40.fullmatch(expected_source_commit):
        fail(f"invalid expected source commit {expected_source_commit!r}")

    for key, expected in EXPECTED_FIXED_INPUTS.items():
        actual = require_key(manifest, key, manifest_path)
        if actual != expected:
            fail(f"{manifest_path}: {key}={actual!r} != expected {expected!r}")

    if require_key(manifest, "candidate", manifest_path) != candidate:
        fail(f"{manifest_path}: candidate mismatch")
    if not require_key(manifest, "gl_context", manifest_path).strip():
        fail(f"{manifest_path}: GL context identity is empty")
    if require_key(manifest, "captured", manifest_path) != "true":
        fail(f"{manifest_path}: captured gate is not true")
    error = manifest.get("error", "")
    if error:
        fail(f"{manifest_path}: capture recorded error: {error}")

    source_commit = require_key(manifest, "source_commit", manifest_path).lower()
    if source_commit != expected_source_commit:
        fail(
            f"{manifest_path}: source_commit {source_commit} "
            f"!= expected {expected_source_commit}"
        )
    harness_state = require_key(
        manifest, "source_commit_harness_state", manifest_path
    )
    if harness_state != "dirty":
        fail(
            f"{manifest_path}: parity-adapter capture must report a dirty "
            f"harness overlay, got {harness_state!r}"
        )

    manifest_harness = require_key(
        manifest, "golden_capture_harness_sha256", manifest_path
    ).lower()
    manifest_cpp_sha = require_key(
        manifest, "golden_capture_cpp_sha256", manifest_path
    ).lower()
    manifest_cpp_blob = require_key(
        manifest, "golden_capture_cpp_git_blob", manifest_path
    ).lower()
    if not HEX64.fullmatch(manifest_harness):
        fail(f"{manifest_path}: malformed harness SHA-256")
    if not HEX64.fullmatch(manifest_cpp_sha):
        fail(f"{manifest_path}: malformed GoldenCapture.cpp SHA-256")
    if not HEX40.fullmatch(manifest_cpp_blob):
        fail(f"{manifest_path}: malformed GoldenCapture.cpp git blob")
    if manifest_harness != harness.composite_sha256:
        fail(
            f"{manifest_path}: manifest harness hash {manifest_harness} "
            f"!= recomputed {harness.composite_sha256}"
        )
    if manifest_cpp_sha != harness.cpp_sha256:
        fail(f"{manifest_path}: GoldenCapture.cpp SHA-256 does not match source bytes")
    if manifest_cpp_blob != harness.cpp_git_blob:
        fail(f"{manifest_path}: GoldenCapture.cpp git blob does not match source bytes")

    parity_state = require_key(manifest, "cross_revision_parity", manifest_path)
    if not parity_state.startswith("unestablished"):
        fail(
            f"{manifest_path}: capture manifest unexpectedly self-asserts "
            f"cross-revision parity: {parity_state!r}"
        )

    required = parse_dimensions(
        require_key(manifest, "required_dimensions", manifest_path),
        f"{manifest_path}:required_dimensions",
    )
    for key in ("actual_framebuffer", "actual_client", "actual_scene"):
        actual = parse_dimensions(
            require_key(manifest, key, manifest_path),
            f"{manifest_path}:{key}",
        )
        if actual != required:
            fail(f"{manifest_path}: {key} {actual} != required {required}")

    image_name = require_key(manifest, "image", manifest_path).replace("\\", "/").split("/")[-1]
    if image_name != image_path.name:
        fail(
            f"{manifest_path}: image manifest basename {image_name!r} "
            f"!= {image_path.name!r}"
        )

    total_pixels = parse_positive_int(
        require_key(manifest, "capture_rgb_total_pixels", manifest_path),
        f"{manifest_path}:capture_rgb_total_pixels",
    )
    lit_pixels = parse_positive_int(
        require_key(manifest, "capture_rgb_lit_pixels", manifest_path),
        f"{manifest_path}:capture_rgb_lit_pixels",
    )
    if total_pixels != required[0] * required[1]:
        fail(f"{manifest_path}: capture pixel count does not match dimensions")
    if lit_pixels > total_pixels:
        fail(f"{manifest_path}: lit pixel count exceeds total pixel count")
    parse_positive_int(
        require_key(manifest, "capture_rgb_fnv1a64", manifest_path),
        f"{manifest_path}:capture_rgb_fnv1a64",
    )

    final_gate = require_key(manifest, "final_stage_gate", manifest_path)
    control = require_key(manifest, "control", manifest_path)
    if candidate in FINAL_STAGE_CANDIDATES:
        if final_gate != "verified":
            fail(f"{manifest_path}: final-stage gate is not verified")
        if not control or control == "none":
            fail(f"{manifest_path}: final-stage candidate has no control")
        parse_positive_int(
            require_key(manifest, "control_rgb_fnv1a64", manifest_path),
            f"{manifest_path}:control_rgb_fnv1a64",
        )
        parse_positive_int(
            require_key(manifest, "control_rgb_lit_pixels", manifest_path),
            f"{manifest_path}:control_rgb_lit_pixels",
        )
        parse_positive_int(
            require_key(manifest, "control_changed_pixels", manifest_path),
            f"{manifest_path}:control_changed_pixels",
        )
    else:
        if final_gate != "not-applicable-or-not-reached":
            fail(f"{manifest_path}: synthetic candidate has unexpected final-stage gate")
        if control != "none":
            fail(f"{manifest_path}: synthetic candidate unexpectedly has a control")

    if candidate == "fade":
        expected_fade = {
            "fade_target_type": "FadeOutWhite",
            "fade_target_color": "1",
            "fade_target_percent": "0.5",
            "fade_update_observed": "true",
            "fade_update_percent": "0.5",
            "fade_draw_observed": "true",
            "fade_draw_color": "1",
            "fade_draw_percent": "0.5",
            "fade_update_postcondition_verified": "true",
            "fade_half_white_rgb_verified": "true",
        }
        for key, expected in expected_fade.items():
            actual = require_key(manifest, key, manifest_path)
            if actual != expected:
                fail(
                    f"{manifest_path}: fade fixture gate {key}={actual!r} "
                    f"!= {expected!r}"
                )

        expected_hook = "false" if source_commit == PHASE3_BASELINE else "true"
        for key in ("fade_update_hook_observed", "fade_draw_hook_observed"):
            actual = require_key(manifest, key, manifest_path)
            if actual != expected_hook:
                fail(
                    f"{manifest_path}: {key}={actual!r} != expected "
                    f"{expected_hook!r} for source commit {source_commit}"
                )

        parse_positive_int(
            require_key(manifest, "fade_draw_type_value", manifest_path),
            f"{manifest_path}:fade_draw_type_value",
        )


def load_capture_evidence(
    directory: pathlib.Path,
    candidate: str,
    expected_source_commit: str,
    harness: HarnessIdentity,
) -> CaptureEvidence:
    manifest_path = directory / f"{candidate}.txt"
    image_path = directory / f"{candidate}.png"
    manifest = parse_manifest(manifest_path)
    validate_manifest(
        candidate,
        manifest,
        manifest_path,
        image_path,
        expected_source_commit,
        harness,
    )
    png = decode_png_rgb(image_path)
    expected_dimensions = parse_dimensions(
        manifest["required_dimensions"],
        f"{manifest_path}:required_dimensions",
    )
    if (png.width, png.height) != expected_dimensions:
        fail(
            f"{image_path}: decoded dimensions {(png.width, png.height)} "
            f"!= manifest {expected_dimensions}"
        )
    decoded_lit = _lit_pixels(png.rgb)
    manifest_lit = int(manifest["capture_rgb_lit_pixels"], 10)
    if decoded_lit != manifest_lit:
        fail(
            f"{image_path}: decoded lit pixels {decoded_lit} "
            f"!= manifest {manifest_lit}"
        )
    return CaptureEvidence(
        candidate=candidate,
        manifest_path=manifest_path,
        image_path=image_path,
        manifest=manifest,
        png=png,
        image_sha256=_sha256_bytes(image_path.read_bytes()),
    )


def compare_capture_evidence(left: CaptureEvidence, right: CaptureEvidence) -> None:
    if left.candidate != right.candidate:
        fail("internal candidate mismatch")
    candidate = left.candidate
    for key in COMMON_INPUT_KEYS:
        left_value = require_key(left.manifest, key, left.manifest_path)
        right_value = require_key(right.manifest, key, right.manifest_path)
        if left_value != right_value:
            fail(
                f"{candidate}: fixture input {key!r} differs: "
                f"{left_value!r} != {right_value!r}"
            )

    for key in (
        "golden_capture_harness_sha256",
        "golden_capture_cpp_sha256",
        "golden_capture_cpp_git_blob",
        "capture_rgb_fnv1a64",
        "capture_rgb_lit_pixels",
        "capture_rgb_total_pixels",
    ):
        if left.manifest[key].lower() != right.manifest[key].lower():
            fail(
                f"{candidate}: parity evidence field {key!r} differs: "
                f"{left.manifest[key]!r} != {right.manifest[key]!r}"
            )

    if candidate in FINAL_STAGE_CANDIDATES:
        for key in (
            "control_rgb_fnv1a64",
            "control_rgb_lit_pixels",
            "control_changed_pixels",
        ):
            if left.manifest[key] != right.manifest[key]:
                fail(
                    f"{candidate}: final-stage control field {key!r} differs: "
                    f"{left.manifest[key]!r} != {right.manifest[key]!r}"
                )

    if candidate == "fade":
        for key in (
            "fade_target_type",
            "fade_target_color",
            "fade_target_percent",
            "fade_update_observed",
            "fade_update_percent",
            "fade_draw_observed",
            "fade_draw_type_value",
            "fade_draw_color",
            "fade_draw_percent",
            "fade_update_postcondition_verified",
            "fade_half_white_rgb_verified",
        ):
            if left.manifest[key] != right.manifest[key]:
                fail(
                    f"{candidate}: fade fixture field {key!r} differs: "
                    f"{left.manifest[key]!r} != {right.manifest[key]!r}"
                )

    if (left.png.width, left.png.height) != (right.png.width, right.png.height):
        fail(f"{candidate}: decoded image dimensions differ")
    if left.png.rgb != right.png.rgb:
        changed = sum(
            1
            for i in range(0, len(left.png.rgb), 3)
            if left.png.rgb[i : i + 3] != right.png.rgb[i : i + 3]
        )
        fail(
            f"{candidate}: decoded RGB differs at {changed} pixel(s); "
            f"phase3_png_sha256={left.image_sha256}, "
            f"phase4_png_sha256={right.image_sha256}"
        )


def validate_parity(
    phase3_root: pathlib.Path,
    phase4_root: pathlib.Path,
    phase3_capture_dir: pathlib.Path,
    phase4_capture_dir: pathlib.Path,
    phase4_commit: str,
    candidates: Sequence[str],
    *,
    check_git: bool = True,
) -> list[tuple[str, str]]:
    phase4_commit = phase4_commit.lower()
    if not HEX40.fullmatch(phase4_commit):
        fail(f"invalid Phase 4 commit {phase4_commit!r}")
    if phase4_commit == PHASE3_BASELINE:
        fail("Phase 4 commit must differ from the Phase 3 baseline")

    if check_git:
        check_source_checkout(
            phase3_root,
            PHASE3_BASELINE,
            allow_harness_overlay=True,
        )
        check_source_checkout(
            phase4_root,
            phase4_commit,
            allow_harness_overlay=True,
        )

    phase3_harness = compute_harness_identity(phase3_root)
    phase4_harness = compute_harness_identity(phase4_root)
    if phase3_harness != phase4_harness:
        fail(
            "GoldenCapture harness bytes differ between source roots: "
            f"phase3={phase3_harness.composite_sha256}, "
            f"phase4={phase4_harness.composite_sha256}"
        )

    results: list[tuple[str, str]] = []
    for candidate in candidates:
        left = load_capture_evidence(
            phase3_capture_dir,
            candidate,
            PHASE3_BASELINE,
            phase3_harness,
        )
        right = load_capture_evidence(
            phase4_capture_dir,
            candidate,
            phase4_commit,
            phase4_harness,
        )
        compare_capture_evidence(left, right)
        results.append((candidate, left.image_sha256))
    return results


def _png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", crc)
    )


def _write_test_png(path: pathlib.Path, width: int, height: int, rgb: bytes) -> None:
    if len(rgb) != width * height * 3:
        raise AssertionError("bad test RGB length")
    scanlines = bytearray()
    row_bytes = width * 3
    for y in range(height):
        scanlines.append(0)
        start = y * row_bytes
        scanlines.extend(rgb[start : start + row_bytes])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
        + _png_chunk(b"IEND", b"")
    )


def _test_manifest(
    candidate: str,
    source_commit: str,
    harness: HarnessIdentity,
    image_path: pathlib.Path,
    lit_pixels: int,
    total_pixels: int,
) -> str:
    control = "none"
    final_gate = "not-applicable-or-not-reached"
    extra: list[tuple[str, str]] = []
    if candidate in FINAL_STAGE_CANDIDATES:
        control = "same-update control"
        final_gate = "verified"
        extra.extend(
            (
                ("control_rgb_fnv1a64", "111"),
                ("control_rgb_lit_pixels", str(lit_pixels)),
                ("control_changed_pixels", "1"),
            )
        )
    if candidate == "fade":
        hook_observed = "false" if source_commit == PHASE3_BASELINE else "true"
        extra.extend(
            (
                ("fade_target_type", "FadeOutWhite"),
                ("fade_target_color", "1"),
                ("fade_target_percent", "0.5"),
                ("fade_update_observed", "true"),
                ("fade_update_percent", "0.5"),
                ("fade_draw_observed", "true"),
                ("fade_draw_type_value", "4"),
                ("fade_draw_color", "1"),
                ("fade_draw_percent", "0.5"),
                ("fade_update_hook_observed", hook_observed),
                ("fade_update_postcondition_verified", "true"),
                ("fade_draw_hook_observed", hook_observed),
                ("fade_half_white_rgb_verified", "true"),
            )
        )
    values = {
        "phase": "4",
        "candidate": candidate,
        "fixture_contract": FIXTURE_CONTRACT,
        "parity_adapter_contract": PARITY_ADAPTER_CONTRACT,
        "plan_baseline": "bb8f619da7abbe614ea60765006f290a60938f98",
        "phase_plan_blob": PHASE_PLAN_BLOB,
        "phase3_baseline_commit": PHASE3_BASELINE,
        "source_commit": source_commit,
        "source_commit_harness_state": "dirty",
        "golden_capture_cpp_git_blob": harness.cpp_git_blob,
        "golden_capture_cpp_sha256": harness.cpp_sha256,
        "golden_capture_harness_sha256": harness.composite_sha256,
        "cross_revision_parity": "unestablished; test fixture",
        "map": "TEST ARENA",
        "mode": "Battle",
        "hunter": "Samus",
        "recolor": "0",
        "required_dimensions": "1600x900",
        "actual_framebuffer": "1600x900",
        "actual_client": "1600x900",
        "actual_scene": "1600x900",
        "warmup_updates": "12",
        "capture_update_ordinal": "13",
        "trigger": "same deterministic trigger",
        "camera_mode": "player-debug-override",
        "camera_position": "0,16,30",
        "camera_target": "0,1,0",
        "fov_degrees": "78",
        "synthetic_fixture_space": "camera-relative",
        "resolution_scale": "100",
        "lighting": "on",
        "cel": "off",
        "cel_bands": "8",
        "cel_edge": "0.5",
        "fog": "off",
        "texture_filtering": "off",
        "gl_context": "test GL 4.6 compatibility / fixture GPU",
        "fixture": f"fixture {candidate}",
        "fixture_scope": "test",
        "final_stage_gate": final_gate,
        "control": control,
        "capture_rgb_fnv1a64": "222",
        "capture_rgb_lit_pixels": str(lit_pixels),
        "capture_rgb_total_pixels": str(total_pixels),
        "captured": "true",
        "image": str(image_path),
    }
    values.update(extra)
    return "".join(f"{key}={value}\n" for key, value in values.items())


def self_test() -> None:
    phase4_commit = "1" * 40
    with tempfile.TemporaryDirectory(prefix="golden-parity-selftest-") as temp_name:
        temp = pathlib.Path(temp_name)
        root3 = temp / "root3"
        root4 = temp / "root4"
        cap3 = temp / "cap3"
        cap4 = temp / "cap4"
        for root in (root3, root4):
            for relative in HARNESS_RELATIVE_PATHS:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((relative.as_posix() + "\nshared harness\n").encode("utf-8"))
        cap3.mkdir()
        cap4.mkdir()
        harness = compute_harness_identity(root3)
        if harness != compute_harness_identity(root4):
            raise AssertionError("self-test harness identities differ")

        width, height = 1600, 900
        rgb = bytes((12, 24, 36)) * (width * height)
        lit_pixels = width * height
        total_pixels = width * height
        for candidate in CANDIDATES:
            for directory, source_commit in (
                (cap3, PHASE3_BASELINE),
                (cap4, phase4_commit),
            ):
                image_path = directory / f"{candidate}.png"
                _write_test_png(image_path, width, height, rgb)
                (directory / f"{candidate}.txt").write_text(
                    _test_manifest(
                        candidate,
                        source_commit,
                        harness,
                        image_path,
                        lit_pixels,
                        total_pixels,
                    ),
                    encoding="utf-8",
                )

        passed = validate_parity(
            root3,
            root4,
            cap3,
            cap4,
            phase4_commit,
            CANDIDATES,
            check_git=False,
        )
        if len(passed) != len(CANDIDATES):
            raise AssertionError("self-test valid pair did not pass")

        hud_manifest = cap4 / "hud.txt"
        original = hud_manifest.read_text(encoding="utf-8")
        hud_manifest.write_text(
            original.replace(
                f"golden_capture_harness_sha256={harness.composite_sha256}",
                "golden_capture_harness_sha256=" + ("0" * 64),
            ),
            encoding="utf-8",
        )
        try:
            validate_parity(
                root3,
                root4,
                cap3,
                cap4,
                phase4_commit,
                ("hud",),
                check_git=False,
            )
        except GateError:
            pass
        else:
            raise AssertionError("self-test mismatched harness was accepted")
        hud_manifest.write_text(original, encoding="utf-8")

        fade_image = cap4 / "fade.png"
        altered_rgb = bytearray(rgb)
        altered_rgb[0] ^= 1
        _write_test_png(fade_image, width, height, bytes(altered_rgb))
        try:
            validate_parity(
                root3,
                root4,
                cap3,
                cap4,
                phase4_commit,
                ("fade",),
                check_git=False,
            )
        except GateError:
            pass
        else:
            raise AssertionError("self-test pixel mismatch was accepted")

    print("Golden parity validator self-test passed")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate fail-closed Phase 3 vs Phase 4 GoldenCapture parity evidence."
        )
    )
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--phase3-root", type=pathlib.Path)
    parser.add_argument("--phase4-root", type=pathlib.Path)
    parser.add_argument("--phase3-captures", type=pathlib.Path)
    parser.add_argument("--phase4-captures", type=pathlib.Path)
    parser.add_argument("--phase4-commit")
    parser.add_argument(
        "--candidate",
        action="append",
        choices=CANDIDATES,
        dest="candidates",
        help="Candidate to compare; repeat as needed. Defaults to all seven.",
    )
    args = parser.parse_args(argv)
    if args.self_test:
        return args
    required = (
        "phase3_root",
        "phase4_root",
        "phase3_captures",
        "phase4_captures",
        "phase4_commit",
    )
    missing = [name for name in required if getattr(args, name) is None]
    if missing:
        parser.error(
            "missing required arguments: "
            + ", ".join("--" + name.replace("_", "-") for name in missing)
        )
    return args


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_test:
            self_test()
            return 0

        candidates = tuple(args.candidates) if args.candidates else CANDIDATES
        results = validate_parity(
            args.phase3_root.resolve(),
            args.phase4_root.resolve(),
            args.phase3_captures.resolve(),
            args.phase4_captures.resolve(),
            args.phase4_commit.lower(),
            candidates,
        )
        identity = compute_harness_identity(args.phase3_root.resolve())
        print(f"harness_sha256={identity.composite_sha256}")
        for candidate, image_sha256 in results:
            print(f"PASS {candidate} png_sha256={image_sha256}")
        print(
            "PASS Phase 3 vs Phase 4 decoded RGB parity: "
            f"{len(results)} candidate(s), exact pixel equality"
        )
        return 0
    except GateError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
