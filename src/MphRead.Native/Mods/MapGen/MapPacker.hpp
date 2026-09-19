#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Mods::MapGen
{
    class BuiltMap;
    class MapDefinition;

    class MapPacker final
    {
    public:
        static void Generate(
            BuiltMap* map,
            const std::string& archiveDir,
            const std::string& entityDir,
            const std::string& nodeDir,
            bool verbose = true);

        static void Generate(
            MapDefinition* def,
            const std::string& archiveDir,
            const std::string& entityDir,
            const std::string& nodeDir,
            bool verbose = true);

        [[nodiscard]] static std::int32_t GetPrimaryAxis(
            OpenTK::Mathematics::Vector3 normal) noexcept;

        MapPacker() = delete;
        MapPacker(const MapPacker&) = delete;
        MapPacker(MapPacker&&) = delete;
        MapPacker& operator=(const MapPacker&) = delete;
        MapPacker& operator=(MapPacker&&) = delete;
    };
}
