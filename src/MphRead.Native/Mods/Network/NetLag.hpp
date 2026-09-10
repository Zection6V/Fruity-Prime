#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Network
{
    class NetLag final
    {
    public:
        NetLag() = delete;
        NetLag(const NetLag&) = delete;
        NetLag& operator=(const NetLag&) = delete;

        [[nodiscard]] static std::int32_t RoundTripMs();
        [[nodiscard]] static std::int32_t JitterMs();
        [[nodiscard]] static double LossPercent();
        [[nodiscard]] static bool Active();

        // C# internal static members: exposed to Native translation units because
        // C++ has no assembly-level accessibility corresponding to C# internal.
        [[nodiscard]] static std::int64_t HoldTicks();
        [[nodiscard]] static bool Drops();

        [[nodiscard]] static bool Configure(const std::optional<std::string>& value);
        [[nodiscard]] static bool ConfigureLoss(const std::optional<std::string>& value);
        [[nodiscard]] static std::optional<std::string> Describe();

    private:
        static std::int32_t _roundTripMs;
        static std::int32_t _jitterMs;
        static double _lossPercent;
    };
}
