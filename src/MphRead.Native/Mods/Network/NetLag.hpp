#pragma once

#include "NetFaultQueue.hpp"

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

        [[nodiscard]] static std::int32_t RoundTripMs() noexcept { return _roundTripMs; }
        [[nodiscard]] static std::int32_t JitterMs() noexcept { return _jitterMs; }
        [[nodiscard]] static double LossPercent() noexcept { return _lossPercent; }
        [[nodiscard]] static double ReorderRate() noexcept { return _reorderRate; }
        [[nodiscard]] static double DuplicateRate() noexcept { return _duplicateRate; }
        [[nodiscard]] static std::int32_t Seed() noexcept { return _seed; }
        [[nodiscard]] static bool Active() noexcept;

        template <typename T>
        [[nodiscard]] static NetFaultQueue<T> CreateQueue(bool outbound)
        {
            return NetFaultQueue<T>(static_cast<std::int32_t>(static_cast<std::uint32_t>(_seed) + (outbound ? 1U : 0U)),
                _roundTripMs / 2.0, _jitterMs, _lossPercent / 100, _reorderRate, _duplicateRate);
        }

        [[nodiscard]] static bool ConfigureSeed(const std::optional<std::string>& value);
        [[nodiscard]] static bool ConfigureJitter(const std::optional<std::string>& value);
        [[nodiscard]] static bool ConfigureReorder(const std::optional<std::string>& value);
        [[nodiscard]] static bool ConfigureDuplicate(const std::optional<std::string>& value);
        [[nodiscard]] static bool Configure(const std::optional<std::string>& value);
        [[nodiscard]] static bool ConfigureLoss(const std::optional<std::string>& value);
        [[nodiscard]] static std::optional<std::string> Describe();

    private:
        [[nodiscard]] static bool Rate(const std::optional<std::string>& value, double& rate);

        static std::int32_t _roundTripMs;
        static std::int32_t _jitterMs;
        static double _lossPercent;
        static double _reorderRate;
        static double _duplicateRate;
        static std::int32_t _seed;
    };
}
