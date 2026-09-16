#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MphRead::Mods
{
    class ThumbnailGenerator
    {
    public:
        ThumbnailGenerator() = delete;
        ThumbnailGenerator(const ThumbnailGenerator&) = delete;
        ThumbnailGenerator& operator=(const ThumbnailGenerator&) = delete;

        static constexpr std::int32_t ThumbnailWidth = 1600;
        static constexpr std::int32_t ThumbnailHeight = 900;

        static std::string CacheDirectory();
        static std::string PathFor(const std::string& roomKey);
        static bool Exists(const std::string& roomKey);
        static std::vector<std::string> MultiplayerRooms();
        static bool FirstHuntAvailable();
        static std::vector<std::string> MissingThumbnails();
        static void EnsureCustomPreviews(std::function<void(const std::string&)> report = {});
        static void EnsureCacheDirectory();
    };
}
