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

    bool NetShotEvents::IsRemote(std::int32_t slot) noexcept
    {
        return NetSession::Active() && slot >= 0 && slot < Slots && slot != NetHooks::LocalSlot();
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
                _local.Record(std::max(1U, NetSession::NetFrame()), weapon);
            }
        }
        else if (IsRemote(slot))
        {
            _remote[static_cast<std::size_t>(slot)].Consume();
        }
    }

    void NetShotEvents::PrepareShot(Entities::PlayerEntity& shooter)
    {
        const std::int32_t slot = shooter.SlotIndex();
        if (!IsRemote(slot))
        {
            return;
        }
        Entities::PlayerControls& controls = shooter.Controls();
        if (!controls.Shoot().IsDown() && !controls.Shoot().IsPressed() && !controls.Shoot().IsReleased())
        {
            return;
        }
        const std::optional<::MphRead::BeamType> fired = _remote[static_cast<std::size_t>(slot)].Next();
        if (!fired.has_value() || *fired == shooter.CurrentWeapon())
        {
            return;
        }
        // Equipping resets the charge, and the charge belongs to this shot.
        ::MphRead::EquipInfo& equip = NativeRuntime::RequireReference(shooter.EquipInfo());
        const std::uint16_t charge = equip.ChargeLevel;
        shooter.ModSetWeapon(*fired);
        equip.ChargeLevel = charge;
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
