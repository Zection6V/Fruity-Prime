#include "NoxusIceWaveFreezeSync.hpp"

#include "NoxusIceWaveFreezeRules.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    void NoxusIceWaveFreezeSync::Capture(const Entities::PlayerEntity& player, PlayerState& state)
    {
        state.FrozenFrames = static_cast<std::uint8_t>(std::min<std::uint16_t>(player.ModFrozenFrames(), 0xFF));
    }

    bool NoxusIceWaveFreezeSync::Awaiting(std::int32_t slot)
    {
        if (slot < 0 || static_cast<std::size_t>(slot) >= Slots)
        {
            return false;
        }
        const std::uint32_t at = _predictedAt[static_cast<std::size_t>(slot)];
        if (at == 0)
        {
            return false;
        }
        const std::int32_t local = NetSession::LocalSlot();
        const std::int32_t ping = local >= 0 && static_cast<std::size_t>(local) < NetSession::SlotPing.size()
            ? NetSession::SlotPing[static_cast<std::size_t>(local)] : 0;
        const auto roundTrip = static_cast<std::uint32_t>(std::max(0, (ping * 60 + 999) / 1000));
        return NetSession::NetFrame() - at <= roundTrip + ArrivalSlack;
    }

    void NoxusIceWaveFreezeSync::Apply(Entities::PlayerEntity& player, const PlayerState& state)
    {
        const std::int32_t slot = player.SlotIndex();
        switch (ReconcileFreeze(state.FrozenFrames, player.ModFrozenFrames(), Awaiting(slot)))
        {
        case FreezeStep::Mirror:
            player.ModMirrorFrozen(state.FrozenFrames);
            if (slot >= 0 && static_cast<std::size_t>(slot) < Slots)
            {
                // The authority has it: nothing of this machine's is waiting.
                _predictedAt[static_cast<std::size_t>(slot)] = 0;
            }
            break;
        case FreezeStep::End:
            player.ModMirrorFrozen(0);
            break;
        case FreezeStep::Keep:
            break;
        }
    }

    void NoxusIceWaveFreezeSync::NotePredicted(std::int32_t slot)
    {
        if (slot >= 0 && static_cast<std::size_t>(slot) < Slots)
        {
            _predictedAt[static_cast<std::size_t>(slot)] = std::max(1U, NetSession::NetFrame());
        }
    }

    void NoxusIceWaveFreezeSync::ForgetSlot(std::int32_t slot)
    {
        if (slot >= 0 && static_cast<std::size_t>(slot) < Slots)
        {
            _predictedAt[static_cast<std::size_t>(slot)] = 0;
        }
    }

    void NoxusIceWaveFreezeSync::Reset()
    {
        _predictedAt = {};
    }
}
