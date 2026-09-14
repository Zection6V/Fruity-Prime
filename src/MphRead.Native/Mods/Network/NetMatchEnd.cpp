#include "NetMatchEnd.hpp"

#include "NetLog.hpp"
#include "NetProtocol.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace MphRead::Mods::Network::Detail
{
    [[nodiscard]] bool NetMatchEndNetSessionActive();
    [[nodiscard]] bool NetMatchEndNetSessionIsAuthority();
    [[nodiscard]] bool NetMatchEndNetSessionIsHost();
    [[nodiscard]] std::optional<MatchStatePacket> NetMatchEndNetSessionServerMatch();
    [[nodiscard]] std::uint32_t NetMatchEndNetSessionNetFrame();
    void NetMatchEndNetSessionSendMatchEnd();

    [[nodiscard]] bool NetMatchEndGameStateMatchInProgress();
    void NetMatchEndSetGameStateMatchTime(float value);
    void NetMatchEndGameStateResetMatchProgress();

    void NetMatchEndConsoleWriteLine(std::string_view value);
}

namespace MphRead::Mods::Network
{
    std::uint32_t NetMatchEnd::_lastReport = 0;
    bool NetMatchEnd::_reported = false;
    bool NetMatchEnd::_acknowledged = false;
    std::uint32_t NetMatchEnd::_strandedSince = 0;

    void NetMatchEnd::Reset()
    {
        _lastReport = 0;
        _reported = false;
        _acknowledged = false;
        _strandedSince = 0;
    }

    bool NetMatchEnd::MayEndOnScore()
    {
        return !Detail::NetMatchEndNetSessionActive()
            || Detail::NetMatchEndNetSessionIsAuthority()
            || Detail::NetMatchEndNetSessionIsHost();
    }

    bool NetMatchEnd::InIntermission()
    {
        if (!Detail::NetMatchEndNetSessionActive())
        {
            return false;
        }
        if (!Detail::NetMatchEndGameStateMatchInProgress())
        {
            return true;
        }
        const std::optional<MatchStatePacket> serverMatch
            = Detail::NetMatchEndNetSessionServerMatch();
        return serverMatch.has_value() && serverMatch->Ending();
    }

    void NetMatchEnd::Sync()
    {
        if (!Detail::NetMatchEndNetSessionActive())
        {
            return;
        }

        const std::optional<MatchStatePacket> serverMatch
            = Detail::NetMatchEndNetSessionServerMatch();
        const bool serverEnding = serverMatch.has_value() && serverMatch->Ending();
        if (serverEnding && Detail::NetMatchEndGameStateMatchInProgress())
        {
            Detail::NetMatchEndSetGameStateMatchTime(0.0F);
        }

        RecoverIfStranded(serverEnding);
        if (!Detail::NetMatchEndNetSessionIsAuthority()
            && !Detail::NetMatchEndNetSessionIsHost())
        {
            return;
        }
        if (Detail::NetMatchEndGameStateMatchInProgress())
        {
            _reported = false;
            _acknowledged = false;
            return;
        }
        if (serverEnding)
        {
            _reported = true;
            _acknowledged = true;
            return;
        }
        if (_acknowledged)
        {
            return;
        }
        if (_reported
            && Detail::NetMatchEndNetSessionNetFrame() - _lastReport < ReportInterval)
        {
            return;
        }

        _reported = true;
        _lastReport = Detail::NetMatchEndNetSessionNetFrame();
        Detail::NetMatchEndNetSessionSendMatchEnd();
    }

    void NetMatchEnd::RecoverIfStranded(bool serverEnding)
    {
        const std::optional<MatchStatePacket> state
            = Detail::NetMatchEndNetSessionServerMatch();
        const bool serverRunning = state.has_value() && !serverEnding
            && (state->Flags & MatchStatePacket::FlagInProgress) != 0;
        if (!serverRunning || Detail::NetMatchEndGameStateMatchInProgress())
        {
            _strandedSince = 0;
            return;
        }
        if (_strandedSince == 0)
        {
            _strandedSince = std::max(
                Detail::NetMatchEndNetSessionNetFrame(), std::uint32_t{1});
            return;
        }
        if (Detail::NetMatchEndNetSessionNetFrame() - _strandedSince < StrandedFrames)
        {
            return;
        }

        _strandedSince = 0;
        Detail::NetMatchEndConsoleWriteLine(
            "[net] the server's match is still running; leaving the results screen");
        NetLog::Event("recovered from a results screen the server did not ask for");
        Detail::NetMatchEndGameStateResetMatchProgress();
        Detail::NetMatchEndSetGameStateMatchTime(state->TimeRemaining);
    }

    bool NetMatchEnd::ShouldLeaveAfterMatch()
    {
        return !Detail::NetMatchEndNetSessionActive();
    }
}
