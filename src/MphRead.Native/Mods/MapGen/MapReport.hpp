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

        // The pickups a level holds, as the `items` block a recipe would carry.
        // Prints and writes nothing: a recipe carries comments and is nobody's
        // to rewrite.
        [[nodiscard]] static std::int32_t ListItems(const std::string& target,
            std::optional<std::string> mapName, std::optional<float> forcedScale);

        [[nodiscard]] static std::int32_t ListMaterials(
            const std::string& room);

        MapReport() = delete;

    private:
        [[nodiscard]] static std::string Round(float value);

    public:
        MapReport(const MapReport&) = delete;
        MapReport& operator=(const MapReport&) = delete;
    };
}
