#include "Mods/Network/Replay/DemoPlayback.hpp"
#include "Mods/Network/Replay/DemoMatchBoundary.hpp"

#include "Mods/Network/Protocol/NetProtocol.hpp"
#include "Mods/Network/Session/NetSession.hpp"

#include "NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Mods::Network
{
    std::unique_ptr<DemoReader> DemoPlayback::_reader{};
    std::optional<DemoRecord> DemoPlayback::_pending{};
    std::uint32_t DemoPlayback::_frame = 0;
    bool DemoPlayback::_started = false;
    bool DemoPlayback::_isActive = false;
    std::optional<std::string> DemoPlayback::_lastError{};
    DemoEndFrame DemoPlayback::_endFrame{};
    DemoMatchBoundary DemoPlayback::_matchBoundary{};

    bool DemoPlayback::IsActive() noexcept
    {
        return _isActive;
    }

    bool DemoPlayback::AtEnd() noexcept
    {
        return _isActive && !_pending.has_value();
    }

    bool DemoPlayback::HoldLastFrame() noexcept
    {
        return _isActive && _endFrame.Held();
    }

    void DemoPlayback::NoteWorldStepCompleted() noexcept
    {
        // Called after the final recorded packets have been delivered,
        // dispatched and applied by the scene's simulation step.
        _endFrame.AfterWorldStep(_isActive, AtEnd());
    }

    std::optional<std::string> DemoPlayback::LastError()
    {
        return _lastError;
    }

    bool DemoPlayback::Join(const std::string& path, std::int32_t timeoutMs)
    {
        static_cast<void>(timeoutMs);
        _lastError.reset();
        _endFrame.Reset();
        _matchBoundary.Reset();
        _reader = DemoReader::Open(path);
        if (_reader == nullptr)
        {
            _lastError = "That file isn't a demo this build recognises "
                "(wrong extension, damaged, or from a different build).";
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                "[demo] \"" + path + "\": " + _lastError.value());
            return false;
        }
        if (_reader->ProtocolVersion() != NetConfig::ProtocolVersion)
        {
            const std::string message = "[demo] recorded with protocol "
                + ::MphRead::NativeRuntime::ToString(_reader->ProtocolVersion())
                + ", this build requires protocol "
                + ::MphRead::NativeRuntime::ToString(NetConfig::ProtocolVersion);
            ::MphRead::NativeRuntime::ConsoleWriteLine(message);
            _lastError = "This demo uses an incompatible network protocol.";
            _reader->Dispose();
            _reader = nullptr;
            return false;
        }
        NetSession::StartPlayback();
        _isActive = true;
        _frame = 0;
        _started = false;
        _pending = _reader->ReadNext();
        const bool hadRecords = _pending.has_value();
        std::int64_t knownAt = -1;
        while (_frame < JoinSearchFrames)
        {
            PumpFrame();
            NetSession::Update(static_cast<double>(_frame) / 60.0);
            const auto serverMatch = NetSession::ServerMatch();
            if (serverMatch.has_value() && serverMatch->RoomKey.has_value()
                && !serverMatch->RoomKey->empty())
            {
                if (knownAt < 0)
                {
                    knownAt = static_cast<std::int64_t>(_frame);
                }
                else if (static_cast<std::int64_t>(_frame) - knownAt
                        >= static_cast<std::int64_t>(JoinGraceFrames)
                    || AtEnd())
                {
                    return Rewind(path);
                }
            }
            else if (AtEnd())
            {
                break;
            }
        }
        _lastError = !hadRecords
            ? "That demo file is empty -- nothing was ever recorded to it."
            : "That demo has no match info in its first few seconds -- "
                "the recording may have started before the server said what map it was running.";
        ::MphRead::NativeRuntime::ConsoleWriteLine(
            "[demo] \"" + path + "\": " + _lastError.value());
        Stop();
        return false;
    }

    bool DemoPlayback::Rewind(const std::string& path)
    {
        if (_reader != nullptr)
        {
            _reader->Dispose();
        }
        _reader = DemoReader::Open(path);
        if (_reader == nullptr)
        {
            _lastError = "That demo could not be read a second time.";
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                "[demo] \"" + path + "\": " + _lastError.value());
            Stop();
            return false;
        }
        _frame = 0;
        _started = false;
        _endFrame.Reset();
        _matchBoundary.Reset();
        _pending = _reader->ReadNext();
        NetSession::RewindPlayback();
        return true;
    }

    void DemoPlayback::PumpFrame()
    {
        if (!_isActive || _reader == nullptr || AtEnd())
        {
            // EOF is terminal for this playback: never advance its clock
            // after the last packet and never implicitly rewind or loop.
            return;
        }
        if (_started)
        {
            _frame++;
        }
        _started = true;
        while (_pending.has_value())
        {
            const DemoRecord record = _pending.value();
            if (record.Frame > _frame)
            {
                break;
            }
            const auto data = record.Data;
            // Old demos may contain multiple rounds. Their scene transition
            // has no guaranteed recorded bootstrap, so end safely at the
            // next match boundary, preserving the finished round's picture.
            const auto identity = DemoMatchBoundary::Decode(*data);
            if (identity && _matchBoundary.Observe(identity->MatchId, identity->Epoch))
            {
                NativeRuntime::ConsoleWriteLine(
                    "[demo] legacy multi-match file: stopped at the next match boundary");
                _pending.reset();
                break;
            }
            const std::int32_t length = static_cast<std::int32_t>(data->size());
            NetSession::InjectPlaybackPacket(data, length);
            _pending = _reader->ReadNext();
        }
    }

    void DemoPlayback::Stop()
    {
        _isActive = false;
        if (_reader != nullptr)
        {
            _reader->Dispose();
        }
        _reader.reset();
        _pending.reset();
        _frame = 0;
        _started = false;
        _endFrame.Reset();
        _matchBoundary.Reset();
    }
}
