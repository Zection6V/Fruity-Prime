#include "Mods/Network/Replay/DemoRecorder.hpp"
#include "NativeRuntime/System/DateTime.hpp"
#include "NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/IO.hpp"

#include "Mods/Network/Replay/DemoClip.hpp"
#include "Mods/Network/Replay/DemoFile.hpp"
#include "Mods/Network/Replay/DemoPlayback.hpp"
#include "Mods/Network/Protocol/NetProtocol.hpp"
#include "Mods/Network/Protocol/Packets/FormTransitionPacket.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "Mods/Network/Transport/NetTransport.hpp"
#include "NativeRuntime/System/Stopwatch.hpp"
#include "Read.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace MphRead::Mods::Network
{
    std::unique_ptr<DemoWriter> DemoRecorder::_writer{};
    DemoFrameClock DemoRecorder::_frameClock{};
    std::optional<std::string> DemoRecorder::_currentPath{};
    DemoMatchBoundary DemoRecorder::_matchBoundary{};
    std::string DemoRecorder::_stamp{};
    std::uint32_t DemoRecorder::_segment = 0;
    bool DemoRecorder::_needsRosterBootstrap = false;

    bool DemoRecorder::IsRecording() noexcept
    {
        return _writer != nullptr;
    }

    std::optional<std::string> DemoRecorder::CurrentPath()
    {
        return _currentPath;
    }

    void DemoRecorder::WritePacket(PacketType type, std::span<const std::uint8_t> payload)
    {
        if (!_writer) return;
        std::vector<std::uint8_t> bytes(1 + payload.size());
        bytes[0] = static_cast<std::uint8_t>(type);
        std::copy(payload.begin(), payload.end(), bytes.begin() + 1);
        _writer->WriteRecord(0, bytes);
    }

    void DemoRecorder::WriteRosterBootstrap()
    {
        if (!_writer || !_matchBoundary.HasIdentity()) return;
        // A new round still needs the active players' identities before its
        // first snapshot/roster arrives. The real roster supersedes this copy.
        RosterPacket roster = NetSession::LobbyRoster();
        roster.MatchId = _matchBoundary.MatchId();
        roster.AuthorityEpoch = _matchBoundary.Epoch();
        if (roster.Count == 0) return;
        std::vector<std::uint8_t> payload(RosterPacket::Size);
        roster.Write(payload);
        WritePacket(PacketType::Roster, payload);
        _needsRosterBootstrap = false;
    }

    void DemoRecorder::WriteBootstrap()
    {
        if (!_writer || !_matchBoundary.HasIdentity()) return;
        const auto session = NetSession::ServerSession();
        if (session && session->MatchId == _matchBoundary.MatchId()
            && session->AuthorityEpoch == _matchBoundary.Epoch())
        {
            std::vector<std::uint8_t> payload(SessionStatePacket::Size);
            session->Write(payload);
            WritePacket(PacketType::SessionState, payload);
        }
        const auto match = NetSession::ServerMatch();
        if (match && match->MatchId == _matchBoundary.MatchId()
            && match->AuthorityEpoch == _matchBoundary.Epoch())
        {
            std::vector<std::uint8_t> payload(MatchStatePacket::Size);
            match->Write(payload);
            WritePacket(PacketType::MatchState, payload);
            WriteRosterBootstrap();
        }
    }

    bool DemoRecorder::OpenSegment(std::string_view room)
    {
        const std::string root = SanitizeFileName(room.empty() ? "match" : std::string(room));
        const std::string stem = root + "_" + _stamp + "_m"
            + std::to_string(_matchBoundary.MatchId()) + "_part" + std::to_string(_segment);
        const std::string base = Paths::Combine(Paths::Export(), "_demos", stem);
        std::string path = base + std::string(DemoFile::Extension);
        // In particular, recording twice in the same second cannot overwrite
        // an earlier game or an already finalized round.
        for (std::uint32_t suffix = 2; NativeRuntime::FileExists(path); ++suffix)
        {
            path = base + "_" + std::to_string(suffix) + std::string(DemoFile::Extension);
        }
        try
        {
            _writer = std::make_unique<DemoWriter>(path);
            _currentPath = path;
            _needsRosterBootstrap = true;
            NativeRuntime::ConsoleWriteLine("[demo] recording match "
                + std::to_string(_matchBoundary.MatchId()) + " to " + path);
            return true;
        }
        catch (const std::exception& ex)
        {
            _writer.reset();
            _currentPath.reset();
            NativeRuntime::ConsoleWriteLine(std::string("[demo] recording failed: ") + ex.what());
            return false;
        }
    }

    bool DemoRecorder::Start()
    {
        if (IsRecording() || !NetSession::Active() || DemoPlayback::IsActive()) return false;
        _matchBoundary.Reset();
        if (const auto match = NetSession::ServerMatch(); match)
            (void)_matchBoundary.Observe(match->MatchId, match->AuthorityEpoch);
        else if (const auto session = NetSession::ServerSession(); session)
            (void)_matchBoundary.Observe(session->MatchId, session->AuthorityEpoch);
        _stamp = NativeRuntime::DateTimeToString(
            NativeRuntime::DateTimeNow(), "yyyy-MM-dd_HH-mm-ss");
        _segment = 1;
        if (!OpenSegment(NetSession::ServerMatch().has_value()
            ? NetSession::ServerMatch()->RoomKey.value_or("match") : "match"))
        {
            Stop();
            return false;
        }
        _frameClock.Start(NativeRuntime::StopwatchGetTimestamp(), NativeRuntime::StopwatchFrequency());
        WriteBootstrap();
        return true;
    }

    void DemoRecorder::RotateTo(const DemoMatchBoundary::Identity& identity, std::int64_t arrivedAt)
    {
        const std::string oldPath = _currentPath.value_or("");
        if (_writer) _writer->Dispose();
        _writer.reset();
        _currentPath.reset();
        ++_segment;
        _frameClock.Start(arrivedAt, NativeRuntime::StopwatchFrequency());
        if (OpenSegment(identity.Room))
        {
            WriteBootstrap();
            NativeRuntime::ConsoleWriteLine("[demo] finalized " + oldPath);
        }
        else
        {
            Stop(); // Never continue writing the new round into an old file.
        }
    }

    void DemoRecorder::Stop()
    {
        if (_writer) _writer->Dispose();
        _writer.reset();
        _currentPath.reset();
        _frameClock.Reset();
        _matchBoundary.Reset();
        _stamp.clear();
        _segment = 0;
        _needsRosterBootstrap = false;
    }

    void DemoRecorder::Record(ReceivedPacket packet)
    {
        std::span<const std::uint8_t> data{};
        if (packet.Data == nullptr)
        {
            if (packet.Length != 0)
            {
                throw std::out_of_range(
                    "Specified argument was out of the range of valid values.");
            }
        }
        else
        {
            if (packet.Length < 0
                || static_cast<std::size_t>(packet.Length) > packet.Data->size())
            {
                throw std::out_of_range(
                    "Specified argument was out of the range of valid values.");
            }
            data = std::span<const std::uint8_t>(*packet.Data)
                .first(static_cast<std::size_t>(packet.Length));
        }

        DemoClip::Add(data);
        if (_writer == nullptr)
        {
            return;
        }

        // Record() is called before dispatch. Split on the authoritative
        // incoming match identity even when the next round uses the same map.
        const auto identity = DemoMatchBoundary::Decode(data);
        if (identity && _matchBoundary.Observe(identity->MatchId, identity->Epoch))
        {
            RotateTo(*identity, packet.ArrivedAt);
            if (!_writer) return;
        }
        _writer->WriteRecord(RecordFrame(data, _frameClock.FrameAt(packet.ArrivedAt)), data);
        if (_needsRosterBootstrap && identity
            && (packet.Type() == PacketType::MatchState || packet.Type() == PacketType::MapChange))
        {
            WriteRosterBootstrap();
        }
    }

    void DemoRecorder::RecordOwnIntent(std::int32_t slot, const IntentPacket& intent)
    {
        if (_writer == nullptr && !DemoClip::Active())
        {
            return;
        }

        const std::vector<std::uint8_t> buffer = OwnIntentRecord(slot, intent);
        const std::span<const std::uint8_t> data(buffer);
        DemoClip::Add(data);
        if (_writer != nullptr)
        {
            _writer->WriteRecord(Frame(), data);
        }
    }

    std::vector<std::uint8_t> DemoRecorder::OwnIntentRecord(std::int32_t slot, const IntentPacket& intent)
    {
        // The whole wire form, not FullSize: past it are the shot events and
        // the bombs, and a copy fires nothing its intent does not carry.
        std::vector<std::uint8_t> buffer(2 + intent.WireSize());
        buffer[0] = static_cast<std::uint8_t>(PacketType::SlotIntent);
        buffer[1] = static_cast<std::uint8_t>(slot);
        intent.Write(std::span<std::uint8_t>(buffer).subspan(2));
        return buffer;
    }

    void DemoRecorder::RecordOwnSnapshot(std::span<const std::uint8_t> payload)
    {
        RecordOwnPacket(PacketType::Snapshot, payload);
    }

    void DemoRecorder::RecordOwnPacket(PacketType type, std::span<const std::uint8_t> payload)
    {
        if (_writer == nullptr && !DemoClip::Active())
        {
            return;
        }

        std::vector<std::uint8_t> buffer(1 + payload.size());
        buffer[0] = static_cast<std::uint8_t>(type);
        std::copy(payload.begin(), payload.end(), buffer.begin() + 1);

        const std::span<const std::uint8_t> data(buffer);
        DemoClip::Add(data);
        if (_writer != nullptr)
        {
            _writer->WriteRecord(RecordFrame(data, Frame()), data);
        }
    }

    std::uint32_t DemoRecorder::Frame()
    {
        return _frameClock.FrameAt(::MphRead::NativeRuntime::StopwatchGetTimestamp());
    }

    std::uint32_t DemoRecorder::RecordFrame(
        std::span<const std::uint8_t> packet, std::uint32_t fallbackFrame)
    {
        if (!packet.empty() && packet[0] == static_cast<std::uint8_t>(PacketType::FormTransition))
        {
            FormTransitionPacket forms{};
            if (FormTransitionPacket::TryRead(packet.subspan(1), forms))
                return _frameClock.FrameAtAuthority(forms.Frame, fallbackFrame);
        }
        if (packet.size() < static_cast<std::size_t>(SnapshotHeader::Size + 1)
            || packet[0] != static_cast<std::uint8_t>(PacketType::Snapshot))
        {
            return fallbackFrame;
        }

        const SnapshotHeader snapshot = SnapshotHeader::Read(packet.subspan(1));
        if (!_frameClock.HasAuthorityFrame())
        {
            _frameClock.AnchorAuthorityFrame(snapshot.Frame, fallbackFrame);
        }
        return _frameClock.FrameAtAuthority(snapshot.Frame, fallbackFrame);
    }

    std::string DemoRecorder::SanitizeFileName(std::string name)
    {
#if defined(_WIN32)
        std::replace(name.begin(), name.end(), '"', '_');
        std::replace(name.begin(), name.end(), '<', '_');
        std::replace(name.begin(), name.end(), '>', '_');
        std::replace(name.begin(), name.end(), '|', '_');
        std::replace(name.begin(), name.end(), '\0', '_');
        for (unsigned int value = 1; value < 32; ++value)
        {
            std::replace(name.begin(), name.end(), static_cast<char>(value), '_');
        }
        std::replace(name.begin(), name.end(), ':', '_');
        std::replace(name.begin(), name.end(), '*', '_');
        std::replace(name.begin(), name.end(), '?', '_');
        std::replace(name.begin(), name.end(), '\\', '_');
        std::replace(name.begin(), name.end(), '/', '_');
#else
        std::replace(name.begin(), name.end(), '\0', '_');
        std::replace(name.begin(), name.end(), '/', '_');
#endif
        return name;
    }
}
