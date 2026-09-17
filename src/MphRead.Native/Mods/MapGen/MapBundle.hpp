#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::MapGen
{
    class MapDefinition;

    class MapBundle final
    {
    public:
        static constexpr const char* Extension = ".fpmap";

        [[nodiscard]] static bool Is(const std::string& path);

        [[nodiscard]] static std::string Cook(
            MapDefinition* definition,
            const std::string& recipePath,
            const std::optional<std::string>& outputPath,
            bool verbose = true);

        [[nodiscard]] static std::optional<std::string> ReadRecipe(
            const std::string& bundlePath);

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> ReadEntry(
            const std::string& bundlePath,
            const std::string& name);

        MapBundle() = delete;
        MapBundle(const MapBundle&) = delete;
        MapBundle& operator=(const MapBundle&) = delete;

    private:
        static constexpr const char* LevelDirectory = "maps/";
    };
}
