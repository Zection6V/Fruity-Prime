#!/usr/bin/env python3
"""Phase 9 static audit: the renderer frontend makes no OpenGL call.

The scene renderer (Renderer.cpp, and the parts of Scene that live in
Formats/Movie.cpp and Mods/Render/PreviewPass.cpp) reaches the GPU through
the RHI -- CommandList, GraphicsPipeline, textures, samplers and the shader
constant sink -- and Renderer.hpp does not include the OpenGL wrapper. The
OpenGL calls live in NativeRuntime/Rhi/OpenGL.

Fails on any `GL::` in those files, on Renderer.hpp including OpenTK/GL.hpp,
and on a uniform-location cache or raw GL program id reappearing in Scene.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
NATIVE = ROOT / "src" / "MphRead.Native"
FRONTEND = (
    NATIVE / "Renderer.cpp",
    NATIVE / "Renderer.hpp",
    NATIVE / "Formats" / "Movie.cpp",
    NATIVE / "Mods" / "Render" / "PreviewPass.cpp",
)
GL_CALL = re.compile(r"\bGL::")
RAW_IDS = re.compile(r"_shaderLocations|ShaderProgramId\b|_frameBuffer\b|_screenTexture\b|_renderBuffer\b")


def main() -> int:
    errors: list[str] = []
    for path in FRONTEND:
        text = path.read_text(encoding="utf-8")
        for number, line in enumerate(text.splitlines(), 1):
            if GL_CALL.search(line):
                errors.append(f"{path.relative_to(ROOT)}:{number}: direct OpenGL call: {line.strip()}")
            if RAW_IDS.search(line):
                errors.append(f"{path.relative_to(ROOT)}:{number}: raw GL object in the frontend: {line.strip()}")
    header = (NATIVE / "Renderer.hpp").read_text(encoding="utf-8")
    if "OpenTK/GL.hpp" in header:
        errors.append("src/MphRead.Native/Renderer.hpp includes the OpenGL wrapper")
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print(f"Phase 9 frontend audit passed: {len(FRONTEND)} frontend files, no direct OpenGL.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
