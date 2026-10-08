#pragma once

#include <array>
#include <cstdint>
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

namespace MphRead::Mods::Input
{
    enum class AimOwner : std::uint8_t { Local, Bot, Remote, Spectator };
    enum class AimSource : std::uint8_t { None, Mouse, Dual, Gamepad, Touch, Network, Script };
    enum class AimOperation : std::uint8_t { Pitch, Yaw, Follow, Snap, Projection, Normalize, Shot, Count };

    // Per-player counters. Disabled in normal gameplay; no allocation, formatting
    // or clock query occurs in an axis callback. Diagnostics own serialization.
    struct AimTrace final
    {
        bool Enabled = false;
        AimOwner Owner = AimOwner::Local;
        AimSource Source = AimSource::None;
        std::uint32_t SimulationStep = 0;
        std::uint32_t NativeSequence = 0;
        bool NativeSample = false;
        bool NativeGameplayTick = false;
        OpenTK::Mathematics::Vector3 LastShot{}, LastMuzzle{}, LastTarget{};
        std::array<std::uint32_t, static_cast<std::size_t>(AimOperation::Count)> Calls{};
        void Count(AimOperation operation, std::uint32_t amount = 1) noexcept
        { if (Enabled) Calls[static_cast<std::size_t>(operation)] += amount; }
        void Clear() noexcept { Calls.fill(0); }
        [[nodiscard]] std::uint32_t Get(AimOperation operation) const noexcept
        { return Calls[static_cast<std::size_t>(operation)]; }
    };
}
