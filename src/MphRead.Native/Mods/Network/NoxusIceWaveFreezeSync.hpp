#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Mods::Network
{
    struct PlayerState;

    // The freeze the Judicator's ice wave (Noxus's weapon, in whoever's
    // hands) puts on a player. In a networked match it is the authority's,
    // like a burn: it decides who is frozen and for how long, every snapshot
    // says how many frames each player has left (PlayerState::FrozenFrames),
    // and every copy -- this machine's own player included -- is frozen and
    // iced for exactly that (NoxusIceWaveFreezeRules).
    class NoxusIceWaveFreezeSync final
    {
    public:
        NoxusIceWaveFreezeSync() = delete;

        // Authority: what the snapshot says of `player`'s freeze.
        static void Capture(const Entities::PlayerEntity& player, PlayerState& state);
        // Client: one snapshot's word about it.
        static void Apply(Entities::PlayerEntity& player, const PlayerState& state);
        // This machine's own hit froze its copy of `slot`: kept until the
        // authority's freeze arrives, a round trip later.
        static void NotePredicted(std::int32_t slot);
        static void ForgetSlot(std::int32_t slot);
        static void Reset();

    private:
        static constexpr std::size_t Slots = Entities::PlayerEntity::SlotCapacity;
        // Past a round trip: the snapshot playout hold and jitter.
        static constexpr std::uint32_t ArrivalSlack = 20;

        [[nodiscard]] static bool Awaiting(std::int32_t slot);

        inline static std::array<std::uint32_t, Slots> _predictedAt{};
    };
}
