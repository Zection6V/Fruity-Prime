#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::MapGen
{
    // What a custom map's collision will be, built in memory and written
    // nowhere: against the format's limits, faces that reject part of their
    // own interior, what the surfaces are, what a hand-edited .obj changed,
    // and every drawn surface with nothing solid behind it.
    class MapCheck final
    {
    public:
        MapCheck() = delete;

        [[nodiscard]] static std::int32_t Run(const std::string& room);
    };
}
