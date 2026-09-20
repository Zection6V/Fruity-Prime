#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::MapGen
{
    class MapReport final
    {
    public:
        [[nodiscard]] static std::int32_t ListShaders(
            const std::string& source,
            const std::optional<std::string>& mapName);

        [[nodiscard]] static std::int32_t ListMaterials(
            const std::string& room);

        MapReport() = delete;
        MapReport(const MapReport&) = delete;
        MapReport& operator=(const MapReport&) = delete;
    };
}
