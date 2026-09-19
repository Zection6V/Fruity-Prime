#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class RoomMetadata;
}

namespace MphRead::Mods::MapGen
{
    class MapDefinition;

    class CustomRooms final
    {
    public:
        using DefinitionList = std::vector<std::shared_ptr<MapDefinition>>;

        [[nodiscard]] static const std::string& MapDirectory();
        static void MapDirectory(std::string value);

        [[nodiscard]] static const DefinitionList& Definitions();

        [[nodiscard]] static std::vector<std::string> AppendIds(
            std::vector<std::string> ids);
        [[nodiscard]] static std::vector<std::shared_ptr<MphRead::RoomMetadata>> AppendRooms(
            std::vector<std::shared_ptr<MphRead::RoomMetadata>> rooms);

        [[nodiscard]] static std::string ArchiveDirectory(MapDefinition* def);
        [[nodiscard]] static std::string EntityDirectory();
        [[nodiscard]] static std::string NodeDirectory();

        [[nodiscard]] static std::int32_t GenerateAll(
            bool force = false, bool verbose = true);
        static void GenerateMissing();

        [[nodiscard]] static std::optional<std::string> WhyUnplayable(
            const std::string& roomName);

        CustomRooms() = delete;
        CustomRooms(const CustomRooms&) = delete;
        CustomRooms& operator=(const CustomRooms&) = delete;

    private:
        [[nodiscard]] static std::vector<std::string> MapFiles();
        [[nodiscard]] static std::shared_ptr<DefinitionList> LoadDefinitions();
        [[nodiscard]] static std::shared_ptr<MphRead::RoomMetadata> MakeMetadata(
            MapDefinition* def, std::int32_t id);
        [[nodiscard]] static bool NeedsGenerating(MapDefinition* def);
    };
}
