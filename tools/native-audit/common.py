"""Shared helpers for the C#-to-C++ pitfall scanners.

The scanners are heuristics over source text, not a compiler: every hit is a
place to read, and a clean run means none of the shapes they know about.
See docs/MphRead-Native-CSharp-to-Cpp-Pitfalls.md.
"""
import glob
import os
import re

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
NATIVE = os.path.join(REPO, 'src', 'MphRead.Native') + os.sep
CSHARP = os.path.join(REPO, 'src', 'MphRead') + os.sep


def native_files():
    return sorted(glob.glob(NATIVE + '**/*.cpp', recursive=True)
                  + glob.glob(NATIVE + '**/*.hpp', recursive=True))


def strip(s):
    """Blank out comments and string/char literals, keeping offsets and lines."""
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if s.startswith('//', i):
            j = s.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
            continue
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', s[i:j]))
            i = j
            continue
        if c in '"\'':
            if c == '"' and i > 0 and s[i - 1] == 'R':
                m = re.match(r'"([^(]*)\(', s[i:])
                d = m.group(1) if m else ''
                j = s.find(')' + d + '"', i)
                j = n if j < 0 else j + len(d) + 2
                out.append(re.sub(r'[^\n]', ' ', s[i:j]))
                i = j
                continue
            j = i + 1
            while j < n and s[j] != c:
                j += 2 if s[j] == '\\' else 1
            out.append(c + ' ' * (j - i - 1) + c if j < n else '')
            i = j + 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def line_of(s, pos):
    return s.count('\n', 0, pos) + 1


def rel(path):
    return os.path.relpath(path, REPO)
