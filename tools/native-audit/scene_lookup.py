"""Pitfall 12c: an entity's shared_ptr recovered by walking a scene list
for the element whose .get() equals a raw pointer, and returning or keeping
that element.

C# passes the reference and needs no lookup. The lookup fails for an entity
that is not in the list -- one being spawned (the ice wave), or already
removed -- and C# does not care. Use SharedFrom(entity) (EntityBase.hpp)
when the pointer is kept, and a raw pointer or reference when it is not.
"""
import re
import sys
from common import native_files, strip, line_of, rel

hits = 0
for f in native_files():
    s = strip(open(f, errors='ignore').read())
    for m in re.finditer(r'\b(\w+)\.get\(\)\s*==\s*(\w+)\s*\)', s):
        var = m.group(1)
        after = s[m.end():m.end() + 200]
        if re.search(r'\breturn\s+(?:std::\w+_pointer_cast<[^>]+>\(\s*)?' + var + r'\b'
                     r'|\breturn\s+std::shared_ptr<[^>]+>\(\s*' + var + r'\b'
                     r'|=\s*' + var + r'\s*;', after):
            print(f"{rel(f)}:{line_of(s, m.start())}: {m.group(0)} -- keeps the element the pointer matched")
            hits += 1
print(hits, 'candidate(s)', file=sys.stderr)
