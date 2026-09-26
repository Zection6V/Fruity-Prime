#pragma once

#include "AimAssistTarget.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead
{
    enum class BeamType : std::int8_t;
}

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistTelemetry final
    {
    public:
        AimAssistTelemetry() = delete;

        struct Bucket
        {
            std::string Input{};
            std::string Weapon{};
            std::string Distance{};
            std::int32_t Samples = 0;
            std::int32_t TargetSamples = 0;
            std::int32_t Shots = 0;
            std::int32_t HitEvents = 0;
            std::int64_t ObservedDamage = 0;
            double SecondsOnTarget = 0;
            double AssistSeconds = 0;
            double HeadSeconds = 0;
            double ErrorSum = 0;
            double FrictionSum = 0;
            double CorrectionSum = 0;
            double VelocitySum = 0;
            std::int32_t Switches = 0;

            [[nodiscard]] double HitEventsPerShot() const noexcept { return Shots == 0 ? 0 : HitEvents / static_cast<double>(Shots); }
            [[nodiscard]] double MeanError() const noexcept { return TargetSamples == 0 ? 0 : ErrorSum / TargetSamples; }
            [[nodiscard]] double MeanFriction() const noexcept { return Samples == 0 ? 1 : FrictionSum / Samples; }
        };

        [[nodiscard]] static bool Enabled() noexcept { return _path.has_value(); }
        static void Shot(BeamType weapon);
        static void Hit(const Entities::PlayerEntity* attacker, BeamType weapon, std::uint32_t damage);
        static void Configure(const std::optional<std::string>& path);
        static void Record(BeamType weapon, const AimAssistTarget& target, const AimAssistResult& result,
            float correction, float velocity);

    private:
        static void Save();

        inline static std::array<std::array<std::array<std::shared_ptr<Bucket>, 4>, 16>, 3> Buckets{};
        inline static std::optional<std::string> _path{};
        inline static std::int32_t _lastTarget = -1;
        inline static std::shared_ptr<Bucket> _current{};
        inline static std::array<std::shared_ptr<Bucket>, 16> LastShot{};
    };
}
