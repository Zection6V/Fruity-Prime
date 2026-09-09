#!/usr/bin/env python3
"""Transliterate PlayerAi's behaviour methods from PlayerAi.cs.

Three families and the helpers they call:

  Func1_*   what a bot does as it takes a path through its personality tree
  Func2_*   what it does while it sits in a node -- shoot, morph, back off
  Func3_*   the questions it asks to weigh one path against another, each
            answering 1 or 0
  Func4_*   what it does on entering one, which is mostly bookkeeping

plus every `private void Name()` / `Name(AiContext)` helper those reach, so
a converted behaviour calls a real member rather than a name that does not
exist.

Two rules keep the output honest, and both matter more than coverage:

  * a body the converter does not understand *in full* is emitted saying so,
    with the managed source verbatim -- a behaviour that runs the half we
    understood is a bot doing something the game never does; and
  * that verdict propagates along calls.  A body we converted perfectly is
    still marked unported if anything it calls is unported, because it would
    otherwise run to completion with a step silently missing.

    python tools/gen-ai-methods.py

Writes ai_funcs1.generated.cpp, ai_funcs24.generated.cpp and
ai_helpers.generated.cpp under src/MphRead.Native/Entities/Players/, and the
matching declarations under tools/ for pasting into PlayerAi.hpp.
"""

from __future__ import annotations

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import cs_convert  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src" / "MphRead" / "Entities" / "Players" / "PlayerAi.cs"
NATIVE = ROOT / "src" / "MphRead.Native" / "Entities" / "Players"

# `private void Name()` and `private void Name(AiContext context)` are the
# two shapes the converter can give a signature to.  Everything else takes
# arguments this head has no equivalent for yet.
DECLARATION = re.compile(
    r"private void (\w+)\((|AiContext context)\)\s*\r?\n\s*\{")
# The predicates are their own shape: two arguments and an int answer.
PREDICATE = re.compile(
    r"private (?:int|bool) (Func3_\w+)\(AiContext context, "
    r"AiPersonalityData5 param\)")
# ExecuteFuncs3 is a switch expression rather than a switch statement, so
# the identifier-to-predicate mapping is read from it directly.
FUNCS3_CASE = re.compile(r"^(\d+) => (Func3_\w+)\(context, param\),$")
CALLS = re.compile(r"\b(\w+)\((?:|context)\);")
CALLS3 = re.compile(r"\b(Func3_\w+)\(context, param\)")

BANNER = [
    "    // NOT PORTED.  The managed body has lines the converter did not",
    "    // understand, or calls something that is itself not ported, and a",
    "    // half-converted behaviour would run the parts it did understand",
    "    // without the parts it did not.  The managed source follows.",
]


class Method:
    def __init__(self, name: str, takes_context: bool, source: list[str],
                 predicate: bool = False):
        self.name = name
        self.predicate = predicate
        self.takes_context = takes_context
        self.source = source
        self.code: list[str] = []
        self.self_ok = False
        self.ok = False
        self.calls: set[str] = set()

    @property
    def member(self) -> str:
        return self.name[0].lower() + self.name[1:]

    def signature(self, qualified: bool) -> list[str]:
        kind = "int" if self.predicate else "void"
        # Only a predicate returns anything, so only a predicate can have
        # its answer thrown away.
        attribute = "[[nodiscard]] " if self.predicate else ""
        head = ("%s PlayerAiData::%s(" % (kind, self.member) if qualified
                else "    %s%s %s(" % (attribute, kind, self.member))
        pad = "    " if qualified else "            "
        lines = [head,
                 pad + "const gameplay::Session& session,",
                 pad + "std::uint8_t bot_slot"]
        if self.predicate:
            lines[-1] += ","
            lines.append(pad + "const AiContext& context,")
            lines.append(pad + "const ai::Parameters& parameters")
        elif self.takes_context:
            lines[-1] += ","
            lines.append(pad + "AiContext& context")
        lines[-1] += ") noexcept" + (" {" if qualified else ";")
        return lines


def collect(text: str) -> dict[str, Method]:
    methods: dict[str, Method] = {}
    for match in DECLARATION.finditer(text):
        name = match.group(1)
        if name in methods:
            continue
        body = cs_convert.method_body(text, name)
        if body is None:
            continue
        methods[name] = Method(name, match.group(2) != "",
                               cs_convert.statements(body))
    for match in PREDICATE.finditer(text):
        name = match.group(1)
        if name in methods:
            continue
        body = cs_convert.method_body(text, name)
        if body is None:
            continue
        methods[name] = Method(name, False, cs_convert.statements(body),
                               predicate=True)
    return methods


def family(name: str) -> str | None:
    for prefix in ("Func1_", "Func2_", "Func3_", "Func4_"):
        if name.startswith(prefix):
            return prefix[:-1]
    return None


def reachable(methods: dict[str, Method]) -> set[str]:
    """The behaviours, plus everything they call, transitively."""
    pending = [name for name in methods if family(name) is not None]
    seen = set(pending)
    while pending:
        current = methods[pending.pop()]
        for callee in current.calls:
            if callee not in seen:
                seen.add(callee)
                pending.append(callee)
    return seen


def settle(methods: dict[str, Method], names: set[str]) -> None:
    """Propagate `not understood` from callees to callers until it stops."""
    for name in names:
        methods[name].ok = methods[name].self_ok
    changed = True
    while changed:
        changed = False
        for name in names:
            method = methods[name]
            if not method.ok:
                continue
            for callee in method.calls:
                if callee in names and not methods[callee].ok:
                    method.ok = False
                    changed = True
                    break


def emit(method: Method) -> list[str]:
    lines = method.signature(qualified=True)
    lines.append("    static_cast<void>(session);")
    lines.append("    static_cast<void>(bot_slot);")
    if method.predicate:
        lines.append("    static_cast<void>(context);")
        lines.append("    static_cast<void>(parameters);")
    elif method.takes_context:
        lines.append("    static_cast<void>(context);")
    if method.ok:
        lines.extend(method.code if method.code
                     else ["    // the managed body is empty"])
    else:
        lines.extend(BANNER)
        lines.extend("    // %s" % line for line in method.source)
        if method.predicate:
            # A predicate has to answer something.  The dispatcher below
            # never routes an unported one, so this answer is never the
            # one a bot acts on; it is here because the language needs it.
            lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    return lines


def header(script: str, what: str) -> list[str]:
    return [
        "// Generated by tools/%s from" % script,
        "// src/MphRead/Entities/Players/PlayerAi.cs.  Do not edit by hand.",
        "//",
    ] + ["// " + line for line in what.splitlines()] + [
        '#include "Entities/Players/PlayerAi.hpp"',
        "",
        '#include "Utility/rng.hpp"',
        "",
        "namespace fruityprime::players {",
        "namespace {",
        "",
        "template <typename Flags>",
        "[[nodiscard]] constexpr Flags without(Flags value,",
        "                                      Flags bit) noexcept {",
        "    return static_cast<Flags>(",
        "        static_cast<std::uint32_t>(value)",
        "        & ~static_cast<std::uint32_t>(bit));",
        "}",
        "",
        "",
        "// A Data5 parameter is a fixed-point number wherever it is",
        "// compared against a position.",
        "[[nodiscard]] constexpr float param_float(",
        "    std::int32_t value) noexcept {",
        "    return static_cast<float>(value) / 4096.0F;",
        "}",
        "",
        "} // namespace",
        "",
        "using utility::get_random_int2;",
        "",
    ]


def dispatcher(methods: dict[str, Method], names: list[str],
               suffix: str, takes_context: bool) -> list[str]:
    """The behaviour-index switch, numbered as gen-ai-dispatch.py numbers.

    That table and this switch have to agree on the numbering or every bot
    runs somebody else's behaviour, so the order is derived from the same
    rule -- sorted names, 1-based, 0 meaning "runs nothing" -- rather than
    written out in two places.
    """
    lines = [
        "// Behaviour index to behaviour, numbered the way",
        "// tools/gen-ai-dispatch.py numbers them.",
        "void PlayerAiData::dispatch_funcs%s(" % suffix,
        "    const gameplay::Session& session,",
        "    std::uint8_t bot_slot,",
    ]
    if takes_context:
        lines.append("    AiContext& context,")
    lines.append("    std::uint8_t behavior) noexcept {")
    lines.append("    switch (behavior) {")
    arguments = "session, bot_slot, context" if takes_context \
        else "session, bot_slot"
    for position, name in enumerate(sorted(names)):
        lines.append("    case %d:" % (position + 1))
        lines.append("        %s(%s);" % (methods[name].member, arguments))
        lines.append("        break;")
    lines.append("    default: break;  // 0 runs nothing")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return lines


def funcs3_dispatcher(text: str, methods: dict[str, Method],
                      known: set[str]) -> list[str]:
    """ExecuteFuncs3's identifier-to-predicate mapping.

    Only predicates that are actually ported are routed.  An unported one
    is left to the caller's own fallback rather than answered with a zero
    that would read as a real answer -- `handled` says which happened.
    """
    pairs: list[tuple[int, str]] = []
    for line in text.splitlines():
        match = FUNCS3_CASE.match(line.strip())
        if match and match.group(2) in known:
            pairs.append((int(match.group(1)), match.group(2)))
    lines = [
        "// PlayerAi.ExecuteFuncs3: which predicate each identifier asks.",
        "int PlayerAiData::dispatch_funcs3(",
        "    const gameplay::Session& session,",
        "    std::uint8_t bot_slot,",
        "    const AiContext& context,",
        "    const ai::Parameters& parameters,",
        "    int func_id,",
        "    bool& handled) noexcept {",
        "    handled = true;",
        "    switch (func_id) {",
    ]
    for func_id, name in pairs:
        if not methods[name].ok:
            continue
        lines.append("    case %d:" % func_id)
        lines.append("        return %s(session, bot_slot, context,"
                     % methods[name].member)
        lines.append("                  parameters);")
    lines.append("    default: break;")
    lines.append("    }")
    lines.append("    handled = false;")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    return lines


def write(path: pathlib.Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines), encoding="utf-8", newline="\r\n")


def main() -> int:
    text = SOURCE.read_text(encoding="utf-8")
    methods = collect(text)
    for method in methods.values():
        body = "\n".join(method.source)
        method.calls = {name for name in CALLS.findall(body)
                        if name in methods and name != method.name}
        method.calls |= {name for name in CALLS3.findall(body)
                         if name in methods and name != method.name}
    names = reachable(methods)
    for name in sorted(names):
        method = methods[name]
        method.code, method.self_ok = cs_convert.convert(
            method.source, set(methods))
    settle(methods, names)

    groups = {"Func1": [], "Func2": [], "Func3": [], "Func4": [],
              "helper": []}
    for name in sorted(names):
        groups[family(name) or "helper"].append(name)

    write(NATIVE / "ai_funcs1.generated.cpp",
          header("gen-ai-methods.py",
                 "PlayerAi's Func1_* behaviours, which ExecuteFuncs1\n"
                 "dispatches to.  A body that is not ported says so where\n"
                 "it stands, rather than compiling to a bot that quietly\n"
                 "does nothing.")
          + [line for name in groups["Func1"] for line in emit(methods[name])]
          + dispatcher(methods, groups["Func1"], "1", False)
          + ["} // namespace fruityprime::players", ""])

    write(NATIVE / "ai_funcs24.generated.cpp",
          header("gen-ai-methods.py",
                 "PlayerAi's Func2_* and Func4_* behaviours: what a bot does\n"
                 "while it is in an execution node, and what it does on\n"
                 "entering one.")
          + [line for group in ("Func2", "Func4") for name in groups[group]
             for line in emit(methods[name])]
          + dispatcher(methods, groups["Func2"], "2", True)
          + dispatcher(methods, groups["Func4"], "4", True)
          + ["} // namespace fruityprime::players", ""])

    write(NATIVE / "ai_funcs3.generated.cpp",
          header("gen-ai-methods.py",
                 "PlayerAi's Func3_* predicates: the questions a bot asks\n"
                 "to weigh one path through its personality tree against\n"
                 "another.  Each answers 1 or 0.")
          + [line for name in groups["Func3"] for line in emit(methods[name])]
          + funcs3_dispatcher(text, methods, set(groups["Func3"]))
          + ["} // namespace fruityprime::players", ""])

    write(NATIVE / "ai_helpers.generated.cpp",
          header("gen-ai-methods.py",
                 "The helpers PlayerAi's behaviours call.  These are not\n"
                 "dispatched by identifier; they are reached only from the\n"
                 "behaviours in the other two files.")
          + [line for name in groups["helper"]
             for line in emit(methods[name])]
          + ["} // namespace fruityprime::players", ""])

    declarations: list[str] = []
    for group, label in (("Func1", "ExecuteFuncs1's behaviours"),
                         ("Func2", "ExecuteFuncs2's behaviours"),
                         ("Func3", "ExecuteFuncs3's predicates"),
                         ("Func4", "ExecuteFuncs4's behaviours"),
                         ("helper", "the helpers those call")):
        declarations.append("    // %s." % label)
        for name in groups[group]:
            declarations.extend(methods[name].signature(qualified=False))
    write(ROOT / "tools" / "ai_methods.decl.txt", declarations)

    # A declared name with a NOT PORTED body would otherwise read to
    # tools/port-gap.py as a ported symbol, which would turn that
    # measurement into a way of hiding the thing it exists to measure.
    write(ROOT / "tools" / "ai_unported.txt",
          ["# Generated by tools/gen-ai-methods.py.  PlayerAi methods that",
           "# are declared in the native tree but whose bodies are not",
           "# ported; tools/port-gap.py subtracts these.",
           ]
          + sorted(methods[name].name for name in names
                   if not methods[name].ok)
          + [""])

    for group in ("Func1", "Func2", "Func3", "Func4", "helper"):
        ported = sum(1 for name in groups[group] if methods[name].ok)
        print("%-7s %3d methods, %3d ported"
              % (group, len(groups[group]), ported))
    print("declarations written to tools/ai_methods.decl.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
