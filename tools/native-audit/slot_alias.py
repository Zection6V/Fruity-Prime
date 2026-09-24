"""Pitfall 3: a container element passed by reference (v[i], v.at(), front(),
back()) to a function that removes from that same container, or a reference
to an element held across a push_back/insert/erase of its container.

After the erase the reference names the next element (or freed storage);
C# passes the object, which a List.Remove cannot move.
"""
import re
import sys
from common import native_files, strip, line_of, rel

files = native_files()
src = {f: strip(open(f, errors='ignore').read()) for f in files}
funcs = {}
for f, s in src.items():
    for m in re.finditer(r'\b(\w+)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?:const)?\s*(?:noexcept)?\s*\{', s):
        if m.group(1) in ('if', 'for', 'while', 'switch', 'catch') or '&' not in m.group(2):
            continue
        depth = 1
        i = m.end()
        while i < len(s) and depth:
            depth += s[i] == '{'
            depth -= s[i] == '}'
            i += 1
        funcs.setdefault(m.group(1), []).append(s[m.end():i])
hits = 0
for f, s in src.items():
    for m in re.finditer(r'\b(\w+)\s*\(\s*((?:\*?\w+(?:->|\.))*(\w+))\s*(\[[^\]]*\]|(?:->|\.)(?:at\([^()]*'
                         r'(?:\([^()]*\))?[^()]*\)|front\(\)|back\(\)))\s*\)', s):
        fn, cont = m.group(1), m.group(3)
        for body in funcs.get(fn, []):
            if re.search(r'\b' + re.escape(cont) + r'\b\s*(\.|->)?\s*(erase|pop_back|pop_front|clear|remove|Remove\w*)\b', body) \
                    or re.search(r'(RemoveFirst|erase|Remove\w*)\s*\(\s*\*?\s*' + re.escape(cont) + r'\b', body):
                print(f"{rel(f)}:{line_of(s, m.start())}: {m.group(0)[:110]} -- {fn} removes from {cont}")
                hits += 1
                break
    for m in re.finditer(r'\b(?:auto|[\w:<>]+)\s*&\s*(\w+)\s*=\s*\*?((?:\w+(?:->|\.))*?(\w+))\s*(?:\[[^\]]*\]|'
                         r'(?:->|\.)(?:at\([^;]*?\)|front\(\)|back\(\)))\s*;', s):
        ref, cont = m.group(1), m.group(3)
        depth = 0
        i = m.end()
        while i < len(s):
            if s[i] == '{':
                depth += 1
            elif s[i] == '}':
                if depth == 0:
                    break
                depth -= 1
            i += 1
        body = s[m.end():i]
        g = re.search(r'\b' + re.escape(cont) + r'\b\s*(?:\(\))?\s*(\.|->)\s*(push_back|emplace_back|insert|erase|'
                      r'resize|clear|Add|Insert|Remove\w*|push)\s*\(', body)
        if g and re.search(r'\b' + re.escape(ref) + r'\b', body[g.end():]):
            print(f"{rel(f)}:{line_of(s, m.start())}: {ref} refers into {cont}, which {g.group(2)} changes before {ref} is used")
            hits += 1
print(hits, 'candidate(s)', file=sys.stderr)
