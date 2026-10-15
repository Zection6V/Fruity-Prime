#include "Mods/Entry/CommandLine.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

// ModEntry's flag lookup rules, pinned as they are (refactoring instructions
// 2026-10-15, cases A01-A12). These are the rules every command depends on,
// so a change here is a change to the command line, not a cleanup.
using namespace MphRead::Mods::Entry;

namespace
{
    using Args = std::vector<std::string>;

    bool Check(bool condition, const char* id, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << id << ": " << message << '\n';
        }
        return condition;
    }
}

int main()
{
    bool passed = true;

    passed &= Check(HasFlag(Args{"-FPS"}, "fps"), "A01", "case is ignored");
    passed &= Check(HasFlag(Args{"--fps"}, "fps"), "A02", "every leading hyphen is removed");
    passed &= Check(HasFlag(Args{"fps"}, "fps"), "A03", "a flag with no hyphen still matches");
    passed &= Check(HasFlag(Args{"---fps"}, "fps") && !HasFlag(Args{"-fps-"}, "fps"),
        "A03b", "only leading hyphens are removed");
    passed &= Check(!HasFlag(Args{"-fpscap"}, "fps") && !HasFlag(Args{"-fp"}, "fps"),
        "A03c", "a prefix or extension is a different flag");

    const Args twice{"-name", "first", "-name", "second"};
    passed &= Check(ValueAfter(twice, "name") == std::optional<std::string>("first"),
        "A04", "a repeated flag answers with its first value");
    passed &= Check(ValuesAfter(twice, "name") == Args{"first", "second"},
        "A05", "ValuesAfter returns every value in order");

    passed &= Check(!ValueAfter(Args{"-name"}, "name").has_value(),
        "A06", "a flag at the end has no value");
    passed &= Check(ValuesAfter(Args{"-name"}, "name").empty(),
        "A06b", "a flag at the end has no values");
    passed &= Check(ValueAfter(Args{"-name", "-server"}, "name") == std::optional<std::string>("-server"),
        "A07", "the next token is returned even when it is a flag");
    passed &= Check(IndexOfFlag(Args{"--NaMe", "ABC"}, "name") == 0,
        "A08", "IndexOfFlag ignores hyphens and case");
    passed &= Check(IndexOfFlag(Args{"x", "-name", "-name"}, "name") == 1,
        "A08b", "IndexOfFlag returns the first occurrence");
    passed &= Check(IndexOfFlag(Args{"x"}, "name") == -1 && IndexOfFlag(Args{}, "name") == -1,
        "A08c", "IndexOfFlag returns -1 when absent");
    passed &= Check(ValueAfter(Args{"-name", ""}, "name") == std::optional<std::string>(""),
        "A09", "an empty value is a value");
    passed &= Check(ValuesAfter(Args{"-thumbnail", "A", "-thumbnail", "B"}, "thumbnail") == Args{"A", "B"},
        "A10", "-thumbnail may be given more than once");
    passed &= Check(StartsWithHyphen(ValueAfter(Args{"-fog", "-fps"}, "fog")),
        "A11", "a value that is a flag is visible to the caller");
    passed &= Check(!StartsWithHyphen(std::optional<std::string>()) && !StartsWithHyphen(std::optional<std::string>(""))
        && !StartsWithHyphen(std::optional<std::string>("x-")),
        "A11b", "StartsWithHyphen needs a leading hyphen");
    passed &= Check(HasFlag(Args{"--", "-fps"}, "fps"), "A12", "\"--\" does not end the options");
    passed &= Check(HasFlag(Args{"--"}, ""), "A12b", "\"--\" is the empty flag, matched by nothing else");

    // The value of a repeated flag that is also the value of another one:
    // "-a -a x" gives a's value as "-a" (the next token), not "x".
    passed &= Check(ValueAfter(Args{"-a", "-a", "x"}, "a") == std::optional<std::string>("-a"),
        "A13", "the first occurrence wins even when its value is the flag itself");
    passed &= Check(ValuesAfter(Args{"-a", "-a", "x"}, "a") == Args{"-a", "x"},
        "A13b", "ValuesAfter reads each occurrence's next token");

    if (!passed)
    {
        return 1;
    }
    std::cout << "ModEntry command-line lookup: all cases pass\n";
    return 0;
}
