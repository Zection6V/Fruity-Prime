"""Pitfall 12d: a helper the shared runtime already has, written again in a
file of its own.

The port was done a file at a time, and every file that needed a path, a
flag test or a vector length grew its own copy. The copies drifted: some
read a UTF-8 path in the ANSI code page, some rounded or converted
differently from .NET, some computed a different TestFlag under the same
name. These names now live once (see the table in the doc); a free function
of the same name defined anywhere else is a copy to delete, unless its
parameters are genuinely different (a Matrix4x3, a WTF-8 path type).
"""
import re
import sys
from common import native_files, strip, line_of, rel

SHARED = {
    'NativeRuntime/System/IO': ['PathFromUtf8', 'PathToUtf8', 'PathCombine', 'CombinePath',
                                'FileExists', 'DirectoryExists', 'FileReadAllBytes',
                                'FileWriteAllBytes', 'ReadAllBytes', 'WriteAllBytes'],
    'NativeRuntime/System/Managed': ['RequireReference', 'RoundToEven', 'DotNetRound',
                                     'ConvertToInt32Net9', 'FloatToInt32', 'MathMax', 'MathMin',
                                     'MathClamp', 'HasFlag', 'ManagedAt', 'ManagedListAt',
                                     'UncheckedAdd', 'UncheckedSubtract', 'UncheckedMultiply',
                                     'UncheckedNegate', 'UncheckedIncrement', 'UncheckedDecrement',
                                     'UInt32ToInt32', 'Int32ToUInt32', 'ShiftLeft', 'ShiftRight'],
    'Formats/Types': ['TestFlag', 'TestAny', 'WithX', 'WithY', 'WithZ', 'AddX', 'AddY', 'AddZ'],
    'NativeRuntime/OpenTK/Mathematics': ['DegreesToRadians', 'RadiansToDegrees', 'Length',
                                         'LengthSquared', 'Normalize', 'Multiply', 'Divide', 'Add',
                                         'Subtract', 'Negate', 'Scale', 'ScaleVector', 'Equal',
                                         'IsZero', 'IdentityMatrix', 'CreateScale',
                                         'CreateTranslation', 'CreateRotationX', 'CreateRotationY',
                                         'CreateRotationZ', 'CreateFromAxisAngle', 'ClearScale',
                                         'SetRow3', 'Clamp', 'ComponentMin', 'ComponentMax',
                                         'DistanceSquared', 'Determinant', 'Inverted'],
}


def enclosing_is_class(s, pos):
    stack = []
    for m in re.finditer(r'[{}]', s[:pos]):
        if m.group(0) == '{':
            k = max(s.rfind(';', 0, m.start()), s.rfind('{', 0, m.start()), s.rfind('}', 0, m.start()))
            head = s[k + 1:m.start()]
            if re.search(r'\bnamespace\b', head):
                stack.append('ns')
            elif re.search(r'\b(class|struct|union)\b', head) and not re.search(r'\benum\b', head):
                stack.append('class')
            else:
                stack.append('other')
        elif stack:
            stack.pop()
    return bool(stack) and stack[-1] != 'ns'


hits = 0
for f in native_files():
    path = rel(f)
    if any(path.endswith(home + '.hpp') or path.endswith(home + '.cpp') for home in SHARED):
        continue
    s = strip(open(f, errors='ignore').read())
    for home, names in SHARED.items():
        for m in re.finditer(r'(?m)^[ \t]*(?:\[\[\w+\]\][ \t]*)*(?:(?:static|inline|constexpr)[ \t]+)*'
                             r'(?:[\w:<>,\*& ]*[\w>\*&]|decltype\(auto\))[ \t]+(' + '|'.join(names) + r')\s*\(', s):
            end = s.find(')', m.end())
            after = s[end + 1:end + 60] if end >= 0 else ''
            if not re.match(r'\s*(const\s*)?(noexcept\s*)?\{', after):
                continue
            if enclosing_is_class(s, m.start()):
                continue
            head = ' '.join(s[m.start():end + 1].split())
            print(f"{path}:{line_of(s, m.start())}: {head[:110]} -- {home} has {m.group(1)}")
            hits += 1
print(hits, 'candidate(s)', file=sys.stderr)
