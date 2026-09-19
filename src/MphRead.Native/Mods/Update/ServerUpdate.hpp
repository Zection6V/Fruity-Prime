#pragma once

#include "UpdateCheck.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <ratio>
#include <string>
#include <vector>

namespace MphRead::Mods::Update
{
    class ServerUpdate final
    {
    public:
        using TimeSpan = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

        [[nodiscard]] static bool Enabled() noexcept;
        static void Enabled(bool value) noexcept;

        [[nodiscard]] static TimeSpan Interval() noexcept;
        static void Interval(TimeSpan value) noexcept;

        [[nodiscard]] static std::optional<UpdateInfo> Pending();
        [[nodiscard]] static bool Staged() noexcept;

        [[nodiscard]] static bool AtStartup(const std::vector<std::string>& commandLine);
        [[nodiscard]] static bool ShouldRestart(std::int32_t playerCount);

        ServerUpdate() = delete;
        ServerUpdate(const ServerUpdate&) = delete;
        ServerUpdate& operator=(const ServerUpdate&) = delete;

    private:
        [[nodiscard]] static bool Supervised() noexcept;
        [[nodiscard]] static bool Stage(UpdateInfo update);
        [[nodiscard]] static bool Swap(const std::string& why);
        static void ReplaceInPlace(const std::string& source, const std::string& target);
        static void Displace(const std::string& destination);
        static void SweepOld(const std::string& target) noexcept;
        [[nodiscard]] static bool Restart(const std::string& target) noexcept;
        static void MakeExecutable(const std::string& path) noexcept;

        inline static constexpr const char* IncomingSuffix = ".incoming";
        inline static constexpr const char* OldSuffix = ".fp-old";
    };
}
