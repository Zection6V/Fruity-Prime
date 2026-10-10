#include "NetShotEvents.hpp"

#include "NetHooks.hpp"
#include "NetSession.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <optional>

namespace MphRead::Mods::Network
{
    static_assert(NetShotEvents::Slots == Entities::PlayerEntity::SlotCapacity);

    namespace
    {
        [[nodiscard]] bool Continuous(const Entities::PlayerEntity& shooter) noexcept
        {
            const auto& weapon = NativeRuntime::RequireReference(shooter.EquipInfo()).Weapon;
            return weapon != nullptr && ::MphRead::TestFlag(weapon->Flags, ::MphRead::WeaponFlags::Continuous);
        }
    }

    bool NetShotEvents::Drives(const Entities::PlayerEntity& shooter) noexcept
    {
        const std::int32_t slot = shooter.SlotIndex();
        return NetSession::Active() && slot >= 0 && slot < Slots && slot != NetHooks::LocalSlot()
            && !shooter.IsBot() && _remote[static_cast<std::size_t>(slot)].Active();
    }

    void NetShotEvents::Fired(const Entities::PlayerEntity& shooter, ::MphRead::BeamType weapon, bool continuous) noexcept
    {
        if (continuous || !NetSession::Active())
        {
            return;
        }
        const std::int32_t slot = shooter.SlotIndex();
        if (slot == NetHooks::LocalSlot())
        {
            // The authority's own shots are resolved where they are fired.
            if (!NetSession::IsAuthority())
            {
                _local.Record(std::max(1U, NetSession::NetFrame()), weapon,
                    NativeRuntime::RequireReference(shooter.EquipInfo()).ChargeLevel);
            }
        }
        else if (Drives(shooter))
        {
            _remote[static_cast<std::size_t>(slot)].Consume();
        }
    }

    void NetShotEvents::PrepareShot(Entities::PlayerEntity& shooter)
    {
        if (!Drives(shooter))
        {
            return;
        }
        const std::optional<IntentPacket::ShotEvent> event = _remote[static_cast<std::size_t>(shooter.SlotIndex())].Next();
        if (!event.has_value())
        {
            return;
        }
        const auto weapon = static_cast<::MphRead::BeamType>(event->WeaponId);
        if (weapon != shooter.CurrentWeapon())
        {
            shooter.ModSetWeapon(weapon);
        }
        // After the switch, which resets it: the charge is the shot's.
        NativeRuntime::RequireReference(shooter.EquipInfo()).ChargeLevel = event->Charge;
    }

    bool NetShotEvents::MayFire(const Entities::PlayerEntity& shooter) noexcept
    {
        return !Drives(shooter) || Continuous(shooter)
            || _remote[static_cast<std::size_t>(shooter.SlotIndex())].Next().has_value();
    }

    std::int32_t NetShotEvents::FireReady(Entities::PlayerEntity& shooter)
    {
        // A beam weapon is fired standing: an event met in the ball waits for
        // the copy to stand, or goes stale.
        if (!Drives(shooter) || shooter.IsAltForm() || shooter.IsMorphing())
        {
            return 0;
        }
        RemoteShotQueue& shots = _remote[static_cast<std::size_t>(shooter.SlotIndex())];
        std::int32_t fired = 0;
        for (std::size_t i = 0; i < RemoteShotQueue::Capacity && shots.Next().has_value(); ++i)
        {
            if (!shooter.ModFireShotEvent())
            {
                break;
            }
            ++fired;
        }
        return fired;
    }

    void NetShotEvents::Attach(IntentPacket& intent) noexcept
    {
        _local.Fill(intent, std::max(1U, NetSession::NetFrame()));
    }

    void NetShotEvents::Receive(std::int32_t slot, const IntentPacket& intent) noexcept
    {
        if (slot >= 0 && slot < Slots)
        {
            _remote[static_cast<std::size_t>(slot)].Receive(intent);
        }
    }

    bool NetShotEvents::HasPending(std::int32_t slot) noexcept
    {
        return slot >= 0 && slot < Slots && _remote[static_cast<std::size_t>(slot)].Next().has_value();
    }

    void NetShotEvents::Forget(std::int32_t slot) noexcept
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        _remote[static_cast<std::size_t>(slot)].Reset();
        if (slot == NetHooks::LocalSlot())
        {
            _local.Reset();
        }
    }
}
