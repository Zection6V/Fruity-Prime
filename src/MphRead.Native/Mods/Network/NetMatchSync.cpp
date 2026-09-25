#include "NetMatchSync.hpp"

#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "NetMatchEnd.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Network
{
    std::string NetMatchSync::_lastRoom = "";
    bool NetMatchSync::_everSynced = false;
    float NetMatchSync::_lastDrift = 0.0f;

    void NetMatchSync::Reset()
    {
        _lastRoom = "";
        _everSynced = false;
    }

    bool NetMatchSync::Synced()
    {
        return _everSynced;
    }

    float NetMatchSync::LastDrift()
    {
        return _lastDrift;
    }

    void NetMatchSync::Apply()
    {
        if (!NetSession::Active() || !NetSession::ServerMatch().has_value())
        {
            return;
        }

        const MatchStatePacket state = NetSession::ServerMatch().value();
        const std::string& roomKey = state.RoomKey.value();
        if (roomKey.length() == 0)
        {
            return;
        }

        if (state.PointGoal > 0
            && GameState::PointGoal()
                != static_cast<std::int32_t>(state.PointGoal))
        {
            GameState::PointGoal(
                static_cast<std::int32_t>(state.PointGoal));
        }

        GameState::FriendlyFire(state.FriendlyFire());
        GameState::ShadowFreeze(state.ShadowFreeze());

        if (NetMatchEnd::InIntermission())
        {
            _lastRoom = roomKey;
            return;
        }

        if (state.TimeRemaining <= 0.0f && state.TimeElapsed <= 0.0f)
        {
            return;
        }

        const bool newMatch = roomKey != _lastRoom;
        _lastRoom = roomKey;

        _lastDrift = GameState::MatchTime() - state.TimeRemaining;
        if (newMatch || !_everSynced || std::fabs(_lastDrift) > 1.5f)
        {
            GameState::MatchTime(state.TimeRemaining);
            if (!_everSynced || newMatch)
            {
                std::string message = "[net] match clock synced to server: ";
                message += ::MphRead::NativeRuntime::ToString(state.TimeRemaining, "0");
                message += " s remaining on ";
                message += roomKey;
                NativeRuntime::ConsoleWriteLine(message);
            }
            _everSynced = true;
        }
    }
}
