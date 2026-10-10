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
    // A remote player's shots, as their own machine fired them (protocol 20
    // shot events). The single place the game asks whether a remote copy may
    // fire, and with what.
    //
    // A copy fires each event as it arrives -- with the event's weapon and
    // charge, without waiting for a trigger or a cooldown the owner's machine
    // already kept -- and fires nothing without one. So a copy can neither
    // invent a shot (a trigger held through a respawn) nor change its weapon
    // (the Omega Cannon unequips itself as it fires, and the switch can
    // arrive first), and a shot fired just before its owner was killed still
    // leaves the body. The shot it fires is a picture: hits are the shooter's
    // claims (NetHitClaims), never resolved again from it. Continuous fire
    // makes no events and stays the trigger's; so do bots, which send no
    // intents.
    //
    // Owner side: Fired records the shot, Attach puts the history in the
    // intent. WeaponSelect keeps syncing the weapon held; the damage, the ray
    // and the claims are not touched here.
    class NetShotEvents final
    {
    public:
        static constexpr std::int32_t Slots = 8; // PlayerEntity::SlotCapacity, checked in the .cpp

        // After a shot leaves PlayerEntity::TryFireWeapon. continuous: a Shock
        // Coil frame, which makes no event and spends none.
        static void Fired(const Entities::PlayerEntity& shooter, ::MphRead::BeamType weapon, bool continuous) noexcept;
        // First thing in TryFireWeapon: a copy driven by events takes up the
        // weapon and charge of its oldest unspent event.
        static void PrepareShot(Entities::PlayerEntity& shooter);
        // Whether TryFireWeapon may go on to fire.
        [[nodiscard]] static bool MayFire(const Entities::PlayerEntity& shooter) noexcept;
        // Fires every event waiting for this copy, alive or just killed.
        // Returns how many left.
        static std::int32_t FireReady(Entities::PlayerEntity& shooter);
        static void Attach(IntentPacket& intent) noexcept;
        // Before the frame-order check that refuses an older intent: its
        // movement is stale, its shots are not.
        static void Receive(std::int32_t slot, const IntentPacket& intent) noexcept;
        [[nodiscard]] static bool HasPending(std::int32_t slot) noexcept;
        // A new life, a rejoin, a room change.
        static void Forget(std::int32_t slot) noexcept;

    private:
        [[nodiscard]] static bool Drives(const Entities::PlayerEntity& shooter) noexcept;

        inline static LocalShotLog _local{};
        inline static std::array<RemoteShotQueue, Slots> _remote{};
    };
}
