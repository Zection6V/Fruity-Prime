#pragma once

#include "BuildVersion.hpp"
#include "SyncHttp.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Update
{
    struct UpdateInfo
    {
        std::string Tag;
        MphRead::Mods::Update::Version Version;
        std::string AssetName;
        std::string AssetUrl;
        std::int64_t AssetSize = 0;
        std::string PageUrl;
        std::string Notes;
    };

    class UpdateCheck final
    {
    public:
        [[nodiscard]] static std::optional<std::string> LastReason();

        [[nodiscard]] static std::optional<UpdateInfo> Latest(
            CancellationToken cancel = nullptr);

        [[nodiscard]] static std::optional<UpdateInfo> Parse(
            std::string_view json,
            std::optional<Version> installed = std::nullopt);

        inline static constexpr std::string_view ReleasesPage =
            "https://github.com/liveteklol/Fruity-Prime/releases";

        [[nodiscard]] static bool IsServerBuild() noexcept;
        [[nodiscard]] static std::string Rid();
        [[nodiscard]] static std::string PackageSuffix();
        [[nodiscard]] static std::string BinaryName();

        UpdateCheck() = delete;
        UpdateCheck(const UpdateCheck&) = delete;
        UpdateCheck& operator=(const UpdateCheck&) = delete;

    private:
        static void SetLastReason(std::optional<std::string> reason);
    };
}
