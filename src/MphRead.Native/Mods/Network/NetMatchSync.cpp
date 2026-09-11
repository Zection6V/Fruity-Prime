#include "NetMatchSync.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Network::Detail
{
    // Narrow integration boundary for the C# MatchStatePacket value copied by Apply().
    // RoomKey models the nullable reference held by the C# struct: disengaged means
    // null, so value() below preserves the source's failure before any side effect.
    struct NetMatchSyncMatchStatePacket
    {
        float TimeRemaining = 0.0f;
        float TimeElapsed = 0.0f;
        std::uint8_t Flags = 0;
        std::uint16_t PointGoal = 0;
        std::optional<std::string> RoomKey{};

        [[nodiscard]] bool FriendlyFire() const
        {
            return (Flags & (1u << 2)) != 0;
        }

        [[nodiscard]] bool ShadowFreeze() const
        {
            return (Flags & (1u << 3)) == 0;
        }
    };

    // These are separate reads because the C# source first tests the nullable
    // ServerMatch property and then reads .Value on a second property access.
    bool NetMatchSyncNetSessionActive();
    bool NetMatchSyncNetSessionServerMatchHasValue();
    NetMatchSyncMatchStatePacket NetMatchSyncNetSessionServerMatchValue();

    bool NetMatchSyncNetMatchEndInIntermission();

    std::int32_t NetMatchSyncGameStatePointGoal();
    void NetMatchSyncSetGameStatePointGoal(std::int32_t value);
    void NetMatchSyncSetGameStateFriendlyFire(bool value);
    void NetMatchSyncSetGameStateShadowFreeze(bool value);
    float NetMatchSyncGameStateMatchTime();
    void NetMatchSyncSetGameStateMatchTime(float value);

    // The format seam is deliberately part of the Console boundary: C++ has no
    // System.Globalization equivalent for C#'s current-culture custom numeric
    // format "0". It must return exactly what $"{value:0}" would produce.
    std::string NetMatchSyncConsoleFormatNumber0(float value);
    void NetMatchSyncConsoleWriteLine(std::string_view value);
}

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
        if (!Detail::NetMatchSyncNetSessionActive()
            || !Detail::NetMatchSyncNetSessionServerMatchHasValue())
        {
            return;
        }

        Detail::NetMatchSyncMatchStatePacket state
            = Detail::NetMatchSyncNetSessionServerMatchValue();
        const std::string& roomKey = state.RoomKey.value();
        if (roomKey.length() == 0)
        {
            return;
        }

        if (state.PointGoal > 0
            && Detail::NetMatchSyncGameStatePointGoal()
                != static_cast<std::int32_t>(state.PointGoal))
        {
            Detail::NetMatchSyncSetGameStatePointGoal(
                static_cast<std::int32_t>(state.PointGoal));
        }

        Detail::NetMatchSyncSetGameStateFriendlyFire(state.FriendlyFire());
        Detail::NetMatchSyncSetGameStateShadowFreeze(state.ShadowFreeze());

        if (Detail::NetMatchSyncNetMatchEndInIntermission())
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

        _lastDrift = Detail::NetMatchSyncGameStateMatchTime() - state.TimeRemaining;
        if (newMatch || !_everSynced || std::fabs(_lastDrift) > 1.5f)
        {
            Detail::NetMatchSyncSetGameStateMatchTime(state.TimeRemaining);
            if (!_everSynced || newMatch)
            {
                std::string message = "[net] match clock synced to server: ";
                message += Detail::NetMatchSyncConsoleFormatNumber0(state.TimeRemaining);
                message += " s remaining on ";
                message += roomKey;
                Detail::NetMatchSyncConsoleWriteLine(message);
            }
            _everSynced = true;
        }
    }
}
