#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::MapGen
{
    class Q3Bsp;

    class MapTextureBake final
    {
    public:
        static constexpr std::int32_t DefaultSize = 64;

        class Result final
        {
        public:
            std::int32_t Baked = 0;
            std::shared_ptr<const std::vector<std::string>> Missing;
            std::int64_t Bytes = 0;

            Result();
            Result(const Result&) = delete;
            Result& operator=(const Result&) = delete;
            Result(Result&&) = delete;
            Result& operator=(Result&&) = delete;
        };

        [[nodiscard]] static std::shared_ptr<Result> Bake(
            const std::shared_ptr<Q3Bsp>& bsp,
            const std::shared_ptr<const std::vector<std::optional<std::string>>>& archivePaths,
            const std::optional<std::string>& outputPath,
            std::int32_t size = DefaultSize,
            bool sky = true);

        MapTextureBake() = delete;
        MapTextureBake(const MapTextureBake&) = delete;
        MapTextureBake& operator=(const MapTextureBake&) = delete;
    };
}
