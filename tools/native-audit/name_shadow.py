"""Pitfall 2: an unqualified call to a helper function that names a class in a
namespace searched first, so the call constructs a temporary of that class.

Name lookup from namespace A::B searches A::B, then A, then the global
namespace; a class StorySave in MphRead hides a helper StorySave() at file
scope for every call made inside MphRead::Entities.
"""
import re
import sys
from common import native_files, strip, line_of, rel


def walk(s):
    ns = []
    stack = []
    regions = []
    last = 0
    for m in re.finditer(r'\bnamespace\s+([\w:]*)\s*\{|[{}]', s):
        regions.append((last, m.start(), tuple(n for n in ns if n)))
        if m.group(0).startswith('namespace'):
            parts = m.group(1).split('::') if m.group(1) else ['']
            stack.append(('ns', len(parts)))
            ns.extend(parts)
        elif m.group(0) == '{':
            stack.append(('b', 0))
        elif stack:
            kind, count = stack.pop()
            if kind == 'ns':
                for _ in range(count):
                    ns.pop()
        last = m.end()
    regions.append((last, len(s), tuple(n for n in ns if n)))
    return regions


def ns_at(regions, pos):
    for a, b, p in regions:
        if a <= pos <= b:
            return p
    return ()


files = native_files()
src = {f: strip(open(f, errors='ignore').read()) for f in files}
types = {}
for f, s in src.items():
    r = walk(s)
    for m in re.finditer(r'\b(?:class|struct|enum\s+class|enum)\s+([A-Z]\w*)\b(?:\s*final)?\s*[:{]', s):
        types.setdefault(m.group(1), set()).add(ns_at(r, m.start()))
    for m in re.finditer(r'\busing\s+([A-Z]\w*)\s*=', s):
        types.setdefault(m.group(1), set()).add(ns_at(r, m.start()))
hits = 0
for f, s in src.items():
    if not f.endswith('.cpp'):
        continue
    r = walk(s)
    for m in re.finditer(r'(?m)^[ \t]*(?:\[\[nodiscard\]\][ \t]*)?(?:static[ \t]+|inline[ \t]+|constexpr[ \t]+)*'
                         r'[\w:<>,\*& ]*?[\w>\*&][ \t\*&]+([A-Z]\w*)[ \t]*\([^;{]*\)[^;{]*\{', s):
        name = m.group(1)
        if name not in types:
            continue
        helper_ns = ns_at(r, m.start())
        for c in re.finditer(r'(?<![\w:.>~])' + name + r'\s*\(', s):
            call_ns = ns_at(r, c.start())
            if call_ns[:len(helper_ns)] != helper_ns:
                continue
            before = [call_ns[:k] for k in range(len(call_ns), len(helper_ns), -1)]
            clash = [t for t in types[name] if t in before]
            if clash:
                print(f"{rel(f)}:{line_of(s, c.start())}: {name}() in {'::'.join(call_ns)} resolves to the "
                      f"class in {['::'.join(t) for t in clash]}, not the helper at line {line_of(s, m.start())}")
                hits += 1
print(hits, 'candidate(s)', file=sys.stderr)
