#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The launcher's picture and input checks, in ModEntry's order: -uishot,
    // -uibench, -uidesign, -classicframes, -shellshot, -tapcheck. True when
    // one ran (its result is the process exit code); false, having touched
    // nothing, when none was asked for. A head with no Qt launcher says so
    // and exits 1 rather than failing to build.
    [[nodiscard]] bool TryRunUiCommands(const std::vector<std::string>& args);
}
