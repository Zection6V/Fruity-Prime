#!/usr/bin/env python3
"""Exercise audit masking against calls hidden among C++ literals/comments."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("audit", Path(__file__).with_name("check-phase11-gl-boundaries.py"))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class AuditTests(unittest.TestCase):
    def test_literals_comments_and_line_numbers(self):
        source = '''// GL::Ignored();
/* GL::Ignored(); */
auto s = R"tag(GL::Ignored(); " quoted)tag";
auto t = "escaped \\" GL::Ignored()";
auto n = 1'000; GL::Real();
auto c = 'x'; GL::AlsoReal();
'''
        code = audit.NON_CODE.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source)
        hits = list(audit.GL.finditer(code))
        self.assertEqual([code.count('\n', 0, h.start()) + 1 for h in hits], [5, 6])

    def test_unknown_owners_are_rejected(self):
        self.assertEqual(audit.category('Renderer.cpp'), 'D')
        self.assertEqual(audit.category('Mods/Render/NewOverlay.cpp'), 'D')
        self.assertEqual(audit.category('NativeRuntime/Rhi/OpenGL/NewBackend.cpp'), 'A')
        self.assertEqual(audit.category('NativeRuntime/Skia/SkiaGpu.cpp'), 'B')
        self.assertEqual(audit.category('Mods/Diagnostics/LauncherWindowCheck.cpp'), 'C')

if __name__ == '__main__':
    unittest.main()
