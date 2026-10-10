#pragma once

#include "NetProtocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace MphRead::Mods::Network
{
    // The shot events one remote player has reported, by sequence: what a hit
    // claim naming a shot (HitClaimPacket::ShotSequence) is checked against --
    // the weapon it left with, the world it was aimed in and its ray.
    //
    // Unlike RemoteShotQueue, nothing is spent here: a shot can be named by
    // several claims (splash, ricochets, several victims), and for as long as
    // its projectile can fly.
    class ShotEventLedger final
    {
    public:
        static constexpr std::size_t Capacity = 64;

        void Note(const IntentPacket::ShotEvent& event) noexcept;
        [[nodiscard]] std::optional<IntentPacket::ShotEvent> Find(std::uint32_t sequence) const noexcept;
        void Reset() noexcept { *this = ShotEventLedger{}; }

    private:
        std::array<IntentPacket::ShotEvent, Capacity> _events{};
    };
}
