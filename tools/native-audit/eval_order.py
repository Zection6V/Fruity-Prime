"""Pitfall 1: two side-effecting calls in one expression whose order C++ does
not fix (function arguments in parentheses, operands of + - * etc.).

C# evaluates left to right; MinGW's GCC builds arguments right to left.
Braced initialisers and && || ?: , are sequenced and are skipped.
"""
import re
import sys
from common import native_files, strip, line_of, rel

CALL = re.compile(r'\b(\w*Random\w*|\w*Rng\w*|Read(?!Only)\w*|Next\w*|Pop\w*|Dequeue\w*|Take\w*'
                  r'|Consume\w*|Advance\w*|Skip\w*|GetNext\w*|GetFree\w*|Allocate\w*)\s*\(')
IGNORE = {'ReadOnlySpan', 'ReadOnlyList', 'Readable', 'Random', 'Rng', 'NextIndex', 'Next'}
hits = 0
for f in native_files():
    s = strip(open(f, errors='ignore').read())
    pos = 0
    for m in re.finditer(r'[;{}]', s):
        stmt = s[pos:m.start()]
        start = pos
        pos = m.end()
        for part in re.split(r'&&|\|\||\?|(?<![<>=!:])\s:\s', stmt):
            calls = [c for c in CALL.findall(part) if c not in IGNORE]
            if len(calls) >= 2:
                print(f"{rel(f)}:{line_of(s, start + len(stmt) - len(stmt.lstrip()))}: "
                      f"{calls} :: {' '.join(part.split())[:160]}")
                hits += 1
# std::move(x) and x->... in the same call
for f in native_files():
    s = strip(open(f, errors='ignore').read())
    for m in re.finditer(r'\(\s*(\w+)(->|\.)[^,()]*,\s*std::move\(\1\)', s):
        print(f"{rel(f)}:{line_of(s, m.start())}: moved and read in one call :: {m.group(0)[:120]}")
        hits += 1
print(hits, 'candidate(s)', file=sys.stderr)
