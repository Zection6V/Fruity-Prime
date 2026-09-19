#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MphRead::Mods
{
    class ThumbnailBatch final
    {
    public:
        ThumbnailBatch() = delete;
        ThumbnailBatch(const ThumbnailBatch&) = delete;
        ThumbnailBatch& operator=(const ThumbnailBatch&) = delete;

        [[nodiscard]] static std::int32_t DefaultParallelism();
        [[nodiscard]] static bool CanRun();
        static std::int32_t Run(
            const std::vector<std::string>& rooms,
            std::int32_t parallelism,
            std::int32_t width,
            std::int32_t height,
            std::function<void(const std::string&)> report = {});
    };
}
