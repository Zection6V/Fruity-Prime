#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // -goreabattle starts the first Gorea encounter as a playable Sylux
    // session, without selecting or writing an adventure save slot.
    [[nodiscard]] bool TryRunPlayCommands(const std::vector<std::string>& args);
}
