#pragma once

#include <string>
#include <string_view>

namespace MphRead::Mods::Entry
{
    // `value` resolved against `directory`, absolute and lexically normal.
    // ModEntry passes ConsoleSetup::LaunchDirectory(): a path typed on the
    // command line is relative to where it was typed, while startup has
    // already moved the working directory to the installation.
    [[nodiscard]] std::string FullPathCombine(std::string_view directory, std::string_view value);
}
