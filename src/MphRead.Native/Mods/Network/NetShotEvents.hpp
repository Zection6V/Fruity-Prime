#pragma once

#include "LocalShotLog.hpp"
#include "NetProtocol.hpp"
#include "RemoteShotQueue.hpp"
#include "../../Formats/Enums.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    // Which weapon a remote player's shot is: the one their own machine fired
    // it with, carried as a shot event (protocol 20), never the one their copy
    // here holds when the trigger arrives. The single place the game asks.
    //
    // Firing the Omega Cannon unequips it in the same frame, and the switch
    // that follows can reach other machines before the shot does; reading the
    // held weapon fired a Power Beam in the Omega's place.
    //
    // Owner side: Fired records the shot, Attach puts the history in the
    // intent. Receiving side: Receive takes events from every intent of the
    // player's current life, PrepareShot puts the event's weapon in the copy's
    // hand as it pulls the trigger, Fired spends the event. WeaponSelect keeps
    // syncing the weapon held; the damage, the ray and the claims are not
    // touched here.
    class NetShotEvents final
    {
    public:
        // After a shot leaves PlayerEntity::TryFireWeapon. continuous: a Shock
        // Coil frame, which makes no event and spends none.
        static void Fired(const Entities::PlayerEntity& shooter, ::MphRead::BeamType weapon, bool continuous) noexcept;
        // Before TryFireWeapon decides anything: a remote copy whose trigger is
        // engaged takes up the weapon of its oldest unspent event, keeping the
        // charge it has built.
        static void PrepareShot(Entities::PlayerEntity& shooter);
        static void Attach(IntentPacket& intent) noexcept;
        // Before the frame-order check that refuses an older intent: its
        // movement is stale, its shots are not.
        static void Receive(std::int32_t slot, const IntentPacket& intent) noexcept;
        [[nodiscard]] static bool HasPending(std::int32_t slot) noexcept;
        // A new life, a rejoin, a room change.
        static void Forget(std::int32_t slot) noexcept;

        static constexpr std::int32_t Slots = 8; // PlayerEntity::SlotCapacity, checked in the .cpp

    private:
        [[nodiscard]] static bool IsRemote(std::int32_t slot) noexcept;

        inline static LocalShotLog _local{};
        inline static std::array<RemoteShotQueue, Slots> _remote{};
    };
}
