#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace MphRead::Mods
{
    class ThumbnailLog final
    {
    public:
        ThumbnailLog() = delete;
        ThumbnailLog(const ThumbnailLog&) = delete;
        ThumbnailLog& operator=(const ThumbnailLog&) = delete;

        [[nodiscard]] static std::string Path();
        static void Begin(std::int32_t rooms) noexcept;
        static void Write(const std::string& line);

    private:
        static std::mutex _lock;
        static std::atomic_bool _failed;
    };
}
