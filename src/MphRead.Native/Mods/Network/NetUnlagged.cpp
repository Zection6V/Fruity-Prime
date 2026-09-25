#include "NetUnlagged.hpp"

#include "NetPlayerBridge.hpp"
#include "NetSession.hpp"
#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedIncrement;
using ::MphRead::TestFlag;

namespace MphRead::Mods::Network::Detail
{
    void NetSessionNetUnlaggedReset()
    {
        NetUnlagged::Reset();
    }

    void NetSessionNetUnlaggedRecord(std::uint32_t frame)
    {
        NetUnlagged::Record(frame);
    }
}

namespace MphRead::Mods::Network
{
    void NetUnlagged::Reset()
    {
        _stamp.fill(0);
        _moved.fill(false);
        _newest = 0;
        _reconciled = false;
        _inProgress = false;
        _shooter = nullptr;
        _rewind = 0;
        _shotsCompensated = 0;
        _framesRewound = 0;
        _worstRewind = 0;
        _catchUpSteps = 0;
        _catchUpHits = 0;
        _historyMisses = 0;
    }

    void NetUnlagged::Record(std::uint32_t frame)
    {
        if (!Enabled())
        {
            return;
        }
        Restore();
        _inProgress = false;
        const std::int32_t index = static_cast<std::int32_t>(
            frame % static_cast<std::uint32_t>(HistoryFrames));
        _stamp.at(static_cast<std::size_t>(index)) = frame;
        _newest = frame;
        for (std::int32_t i = 0; i < Slots; ++i)
        {
            if (i >= static_cast<std::int32_t>(Entities::PlayerEntity::Players().size()))
            {
                _inPlay.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index)) = false;
                continue;
            }
            Entities::PlayerEntity& player = RequireReference(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i)));
            const bool active = TestFlag(player.LoadFlags(), Entities::LoadFlags::Active)
                && player.ModIsInPlay();
            _inPlay.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index)) = active;
            if (active)
            {
                _position.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index))
                    = player.Position;
                _altForm.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index))
                    = player.IsAltForm();
            }
        }
    }

    std::int32_t NetUnlagged::RewindFor(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots || slot == NetSession::LocalSlot())
        {
            return 0;
        }
        if (!NetSession::RemoteIntentValid.at(static_cast<std::size_t>(slot)))
        {
            return 0;
        }
        const std::uint32_t ack
            = NetSession::RemoteIntents.at(static_cast<std::size_t>(slot)).AckFrame;
        const std::uint32_t now = NetSession::NetFrame();
        if (ack == 0 || ack >= now)
        {
            return 0;
        }
        std::int64_t depth = static_cast<std::int64_t>(now - ack);
        if (depth > MaxRewindFrames)
        {
            depth = MaxRewindFrames;
        }
        return static_cast<std::int32_t>(depth);
    }

    bool NetUnlagged::Simulating()
    {
        return NetSession::Active()
            && (NetSession::Role() == NetRole::Host || NetSession::IsAuthority());
    }

    void NetUnlagged::BeginShot(Entities::PlayerEntity& shooter)
    {
        if (_inProgress)
        {
            return;
        }
        _shooter = nullptr;
        _rewind = 0;
        if (!Enabled() || !Simulating() || shooter.IsBot())
        {
            return;
        }
        const std::int32_t slot = shooter.SlotIndex();
        const std::int32_t rewind = RewindFor(slot);
        if (rewind <= 0)
        {
            return;
        }
        const std::uint32_t target = NetSession::NetFrame() - static_cast<std::uint32_t>(rewind);
        if (!Reconcile(slot, target))
        {
            _historyMisses = UncheckedIncrement(_historyMisses);
            return;
        }
        _shooter = &shooter;
        _rewind = rewind;
        _shotsCompensated = UncheckedIncrement(_shotsCompensated);
        _framesRewound = UncheckedAdd(_framesRewound, rewind);
        if (rewind > _worstRewind)
        {
            _worstRewind = rewind;
        }

        auto& beams = RequireReference(shooter.EquipInfo()).Beams;
        const std::int32_t length = RequireReference(beams).Length();
        if (static_cast<std::int32_t>(_beamsBefore.size()) < length)
        {
            _beamsBefore = std::vector<bool>(static_cast<std::size_t>(length));
        }
        for (std::int32_t i = 0; i < length; ++i)
        {
            Entities::BeamProjectileEntity& beam
                = RequireReference(RequireReference(beams)[i]);
            _beamsBefore.at(static_cast<std::size_t>(i)) = beam.Lifespan() > 0.0F;
        }
        _inProgress = true;
    }

    bool NetUnlagged::Reconcile(std::int32_t exceptSlot, std::uint32_t frame)
    {
        const std::int32_t index = static_cast<std::int32_t>(
            frame % static_cast<std::uint32_t>(HistoryFrames));
        if (_stamp.at(static_cast<std::size_t>(index)) != frame || frame == 0)
        {
            return false;
        }
        Restore();
        for (std::int32_t i = 0;
            i < Slots && i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++i)
        {
            if (i == exceptSlot
                || !_inPlay.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index)))
            {
                continue;
            }
            Entities::PlayerEntity& player = RequireReference(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i)));
            if (!TestFlag(player.LoadFlags(), Entities::LoadFlags::Active)
                || !player.ModIsInPlay())
            {
                continue;
            }
            const OpenTK::Mathematics::Vector3 was
                = _position.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index));
            if (!std::isfinite(was.X) || !std::isfinite(was.Y) || !std::isfinite(was.Z))
            {
                continue;
            }
            _restore.at(static_cast<std::size_t>(i)) = player.Position;
            _moved.at(static_cast<std::size_t>(i)) = true;
            player.ModPlaceAt(NetPlayerBridge::InFormFor(
                player, was,
                _altForm.at(static_cast<std::size_t>(i)).at(static_cast<std::size_t>(index))));
        }
        _reconciled = true;
        return true;
    }

    void NetUnlagged::Restore()
    {
        if (!_reconciled)
        {
            return;
        }
        for (std::int32_t i = 0;
            i < Slots && i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            ++i)
        {
            if (!_moved.at(static_cast<std::size_t>(i)))
            {
                continue;
            }
            _moved.at(static_cast<std::size_t>(i)) = false;
            RequireReference(Entities::PlayerEntity::Players().at(static_cast<std::size_t>(i)))
                .ModPlaceAt(_restore.at(static_cast<std::size_t>(i)));
        }
        _reconciled = false;
    }

    void NetUnlagged::EndShot(Entities::PlayerEntity& shooter)
    {
        if (_shooter != &shooter || _rewind <= 0)
        {
            Restore();
            _shooter = nullptr;
            _inProgress = false;
            return;
        }
        const std::int32_t slot = shooter.SlotIndex();
        auto& beams = RequireReference(shooter.EquipInfo()).Beams;
        const std::int32_t length = RequireReference(beams).Length();
        std::int32_t newCount = 0;
        for (std::int32_t i = 0; i < length; ++i)
        {
            Entities::BeamProjectileEntity& beam
                = RequireReference(RequireReference(beams)[i]);
            if (!_beamsBefore.at(static_cast<std::size_t>(i)) && beam.Lifespan() > 0.0F)
            {
                _beamsBefore.at(static_cast<std::size_t>(i)) = true;
                ++newCount;
            }
            else
            {
                _beamsBefore.at(static_cast<std::size_t>(i)) = false;
            }
        }
        if (newCount == 0)
        {
            Restore();
            _shooter = nullptr;
            _inProgress = false;
            return;
        }
        for (std::int32_t step = 1; step <= _rewind && newCount > 0; ++step)
        {
            const std::uint32_t frame = NetSession::NetFrame()
                - static_cast<std::uint32_t>(_rewind - step);
            if (!Reconcile(slot, frame))
            {
                if (frame <= _newest)
                {
                    _historyMisses = UncheckedIncrement(_historyMisses);
                    break;
                }
                Restore();
            }
            for (std::int32_t i = 0; i < length; ++i)
            {
                if (!_beamsBefore.at(static_cast<std::size_t>(i)))
                {
                    continue;
                }
                Entities::BeamProjectileEntity& beam
                    = RequireReference(RequireReference(beams)[i]);
                const bool hit = TestFlag(beam.Flags(), Entities::BeamFlags::Collided);
                _catchUpSteps = UncheckedIncrement(_catchUpSteps);
                if (!beam.Process() || TestFlag(beam.Flags(), Entities::BeamFlags::Collided))
                {
                    if (!hit && TestFlag(beam.Flags(), Entities::BeamFlags::Collided))
                    {
                        _catchUpHits = UncheckedIncrement(_catchUpHits);
                    }
                    _beamsBefore.at(static_cast<std::size_t>(i)) = false;
                    --newCount;
                }
            }
        }
        Restore();
        _shooter = nullptr;
        _rewind = 0;
        _inProgress = false;
    }

    std::string NetUnlagged::Describe()
    {
        if (!Enabled())
        {
            return "lag compensation: off";
        }
        if (_shotsCompensated == 0)
        {
            return "lag compensation: on, nothing to compensate (history misses "
                + std::to_string(_historyMisses) + ")";
        }
        const double mean = static_cast<double>(_framesRewound)
            / static_cast<double>(_shotsCompensated);
        return "lag compensation: " + std::to_string(_shotsCompensated)
            + " shots rewound, mean " + ::MphRead::NativeRuntime::ToString(mean, "F1")
            + " frames (" + ::MphRead::NativeRuntime::ToString(mean * 1000.0 / 60.0, "F0")
            + " ms), worst " + std::to_string(_worstRewind)
            + ", catch-up " + std::to_string(_catchUpSteps)
            + " steps / " + std::to_string(_catchUpHits)
            + " hits, history misses " + std::to_string(_historyMisses);
    }
}
