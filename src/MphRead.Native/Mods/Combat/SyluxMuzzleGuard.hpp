#pragma once

#include "../../Formats/Enums.hpp"
#include "BeamObstacleTrace.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <optional>

namespace MphRead::Mods::Combat
{
    // Gun-axis geometry only. The shot direction may differ after disruption.
    class SyluxMuzzleGuard final
    {
    public:
        SyluxMuzzleGuard() = delete;
        // Read once at process startup; FRUITY_SYLUX_MUZZLE_GUARD=0 restores the original path.
        static bool Enabled;
        static constexpr float StartOffset = 0x651 / 4096.0F;
        static constexpr float Length = (0x800 - 0x651) / 4096.0F;

        [[nodiscard]] static constexpr std::optional<OpenTK::Mathematics::Vector3> Start(
            Hunter hunter, OpenTK::Mathematics::Vector3 gunPosition,
            OpenTK::Mathematics::Vector3 aim, OpenTK::Mathematics::Vector3 muzzle,
            OpenTK::Mathematics::Vector3 shotOrigin) noexcept
        {
            using namespace OpenTK::Mathematics;
            if (hunter != Hunter::Sylux) return std::nullopt;
            return Add(Add(gunPosition, ScaleVector(aim, StartOffset)), Subtract(shotOrigin, muzzle));
        }

        [[nodiscard]] static bool ValidSegment(OpenTK::Mathematics::Vector3 start,
            OpenTK::Mathematics::Vector3 end) noexcept
        {
            const auto finite = [](OpenTK::Mathematics::Vector3 v)
            {
                // Room collision indexing converts coordinates to signed integers.
                constexpr float limit = 1'000'000.0F;
                return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)
                    && std::fabs(v.X) <= limit && std::fabs(v.Y) <= limit && std::fabs(v.Z) <= limit;
            };
            if (!finite(start) || !finite(end)) return false;
            const float distanceSquared = OpenTK::Mathematics::LengthSquared(end - start);
            return distanceSquared > 0.0F && distanceSquared <= Length * Length * 1.1F;
        }
    };

    // Opt-in diagnostics: no clocks, samples or string formatting when disabled.
    class SyluxMuzzleGuardMetrics final
    {
    public:
        SyluxMuzzleGuardMetrics() = delete;
        struct Counts final
        {
            std::uint64_t ShotAttempt = 0, AmmoNoSpawn = 0, TraceInvoked = 0, TraceNoHit = 0;
            std::uint64_t MapHit = 0, DoorHit = 0, ForceFieldHit = 0, HitApplied = 0, NetClaimRejected = 0;
            std::array<std::int64_t, 2048> TraceNanoseconds{};
        };
        static bool Enabled;
        static Counts Counters;

        [[nodiscard]] static std::optional<BeamObstacleHit> Trace(
            OpenTK::Mathematics::Vector3 start, OpenTK::Mathematics::Vector3 end, Scene& scene)
        {
            if (!Enabled) return TraceFirstBeamObstacle(start, end, scene);
            const auto before = std::chrono::steady_clock::now();
            auto hit = TraceFirstBeamObstacle(start, end, scene);
            auto& c = Counters;
            c.TraceNanoseconds[c.TraceInvoked % c.TraceNanoseconds.size()]
                = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - before).count();
            ++c.TraceInvoked;
            if (!hit) ++c.TraceNoHit;
            else if (hit->Kind == BeamObstacleKind::Map) ++c.MapHit;
            else if (hit->Kind == BeamObstacleKind::Door) ++c.DoorHit;
            else ++c.ForceFieldHit;
            return hit;
        }
    };
}
