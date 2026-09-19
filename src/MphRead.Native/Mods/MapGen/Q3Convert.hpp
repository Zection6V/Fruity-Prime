#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::MapGen
{
    class MapDefinition;
    class Q3Bsp;

    class Q3Convert final
    {
    public:
        static constexpr float TargetExtent = 130.0F;

        [[nodiscard]] static std::int32_t Run(
            const std::optional<std::string>& source,
            const std::optional<std::string>& mapName,
            const std::optional<std::string>& roomName,
            const std::optional<std::string>& outputDir,
            bool dropClip,
            const std::optional<float>& forcedScale,
            std::int32_t textureSize);

        Q3Convert() = delete;
        Q3Convert(const Q3Convert&) = delete;
        Q3Convert& operator=(const Q3Convert&) = delete;

    private:
        static void Bounds(
            Q3Bsp* bsp,
            std::shared_ptr<std::vector<float>>& min,
            std::shared_ptr<std::vector<float>>& max,
            bool sky);

        [[nodiscard]] static std::int32_t ScaleFactor(
            const std::vector<float>* min,
            const std::vector<float>* max,
            float unit);

        static void AddSpawns(
            MapDefinition* definition,
            Q3Bsp* bsp,
            float unit);

        [[nodiscard]] static float Round(float value) noexcept;

        [[nodiscard]] static std::shared_ptr<std::vector<float>> ParseVector(
            const std::string& value);
    };
}
