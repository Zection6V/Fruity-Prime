#pragma once

#include "SyncHttp.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Update
{
    class UpdateDownload final
    {
    public:
        [[nodiscard]] static std::optional<std::string> LastError();

        [[nodiscard]] static bool Fetch(
            const std::string& url,
            const std::string& path,
            std::int64_t expectedBytes = 0,
            const std::function<void(float)>& progress = {},
            CancellationToken cancel = nullptr);

        UpdateDownload() = delete;
        UpdateDownload(const UpdateDownload&) = delete;
        UpdateDownload& operator=(const UpdateDownload&) = delete;

    private:
        inline static constexpr std::string_view _assetHost =
            "objects.githubusercontent.com";
        inline static constexpr std::string_view _releaseHost = "github.com";
        inline static constexpr std::chrono::minutes _timeout{10};

        [[nodiscard]] static bool IsAllowed(const std::string& url);
        static void SetLastError(std::optional<std::string> value);
    };
}
