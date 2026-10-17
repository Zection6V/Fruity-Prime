#pragma once

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace MphRead::Mods::Network::MapRotationRules
{
    // Catalog and room metadata use canonical keys. A vote overrides this
    // sequence; an unanswered ballot advances deterministically.
    [[nodiscard]] inline std::string NextOrRepeat(
        const std::vector<std::string>& rooms, const std::string& current)
    {
        if (rooms.empty())
        {
            return current;
        }
        const auto found = std::find(rooms.begin(), rooms.end(), current);
        if (found == rooms.end() || std::next(found) == rooms.end())
        {
            return rooms.front();
        }
        return *std::next(found);
    }
}
