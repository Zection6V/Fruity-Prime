#pragma once

#include "UpdateCheck.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>

namespace MphRead::Mods::Update
{
    class Updater final
    {
    public:
        using TimeSpan = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

        [[nodiscard]] static bool Disabled() noexcept;
        static void Disabled(bool value) noexcept;

        [[nodiscard]] static std::optional<UpdateInfo> Available();
        [[nodiscard]] static bool Checked() noexcept;

        [[nodiscard]] static std::optional<UpdateInfo> Check(
            CancellationToken cancel = nullptr);

        static void CheckInBackground(
            std::function<void(UpdateInfo)> found,
            std::function<void()> done = {});

        static void WaitForCheck(TimeSpan limit);

        [[nodiscard]] static bool OpenPage(UpdateInfo update);
        [[nodiscard]] static bool OpenLink(const std::string& url);
        [[nodiscard]] static bool OpenLink(std::nullptr_t url);

        [[nodiscard]] static std::string Describe(UpdateInfo update);

        Updater() = delete;
        Updater(const Updater&) = delete;
        Updater& operator=(const Updater&) = delete;

    private:
        [[nodiscard]] static bool OpenUrl(std::string_view url);
        [[nodiscard]] static bool OpenUrl(std::nullptr_t url);
    };
}
