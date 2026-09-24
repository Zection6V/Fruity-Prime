"""Pitfall 4: a thread_local whose type has a destructor. Under MinGW its
destructor can run after the emulated TLS block is freed. Use
NativeRuntime::ThreadStatic<T>, a plain pointer, or a trivially destructible
type. Lists every thread_local for review; the trivially destructible ones
(integers, raw pointers, std::array of those, std::mt19937) are fine.
"""
import re
import sys
from common import REPO, strip, line_of, rel
import glob

hits = 0
for f in sorted(glob.glob(REPO + '/src/**/*.cpp', recursive=True) + glob.glob(REPO + '/src/**/*.hpp', recursive=True)
                + glob.glob(REPO + '/src/**/*.h', recursive=True)):
    s = strip(open(f, errors='ignore').read())
    for m in re.finditer(r'\bthread_local\b[^;=({]*', s):
        print(f"{rel(f)}:{line_of(s, m.start())}: {' '.join(m.group(0).split())}")
        hits += 1
print(hits, 'thread_local(s) to review', file=sys.stderr)
