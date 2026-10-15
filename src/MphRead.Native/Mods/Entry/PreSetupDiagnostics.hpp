#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The diagnostics ModEntry::TryHandleHeadless answers before it loads a
    // single setting: -rhicontract, the Vulkan/RHI/presentation checks, the
    // shell's window checks and -smoketest, tried in that order. True when
    // one of them ran (its result is the process exit code); false, having
    // touched nothing, when none was asked for.
    //
    // The caller has already applied the render overrides and the start-up
    // window mode, and loads InputSettings only after this returns false.
    [[nodiscard]] bool TryRunPreSetupDiagnostics(const std::vector<std::string>& args);
}
