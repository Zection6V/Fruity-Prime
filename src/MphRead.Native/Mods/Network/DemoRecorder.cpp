#include "DemoRecorder.hpp"

#include "DemoClip.hpp"
#include "DemoFile.hpp"
#include "DemoPlayback.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "NetTransport.hpp"
#include "../../Read.hpp"

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
    std::uint32_t DemoRecorder::_startFrame = 0;
    std::optional<std::string> DemoRecorder::_currentPath{};

    bool DemoRecorder::IsRecording() noexcept
    {
        return _writer != nullptr;
    }

    std::optional<std::string> DemoRecorder::CurrentPath()
    {
        return _currentPath;
    }

    bool DemoRecorder::Start()
    {
        if (IsRecording() || !NetSession::Active() || DemoPlayback::IsActive())
        {
            return false;
        }

        const std::optional<MatchStatePacket> serverMatch = NetSession::ServerMatch();
        const std::string room = SanitizeFileName(
            serverMatch.has_value() && serverMatch->RoomKey.has_value()
                ? serverMatch->RoomKey.value()
                : std::string("match"));

        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#if defined(_WIN32)
        static_cast<void>(localtime_s(&localTime, &nowTime));
#else
        static_cast<void>(localtime_r(&nowTime, &localTime));
#endif
        char timestamp[20]{};
        static_cast<void>(
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &localTime));

        const std::string fileName
            = room + "_" + timestamp + std::string(DemoFile::Extension);
        const std::string& exportPath = Paths::Export();
        const std::string path = Paths::Combine(exportPath, "_demos", fileName);

        try
        {
            _writer = std::make_unique<DemoWriter>(path);
        }
        catch (const std::ios_base::failure& ex)
        {
            std::cout << "[demo] could not start recording: " << ex.what() << '\n';
            _writer.reset();
            return false;
        }

        _currentPath = path;
        _startFrame = NetSession::NetFrame();
        return true;
    }

    void DemoRecorder::Stop()
    {
        if (_writer != nullptr)
        {
            _writer->Dispose();
        }
        _writer.reset();
        _currentPath.reset();
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

        const std::uint32_t frame = Frame();
        _writer->WriteRecord(frame, data);
    }

    void DemoRecorder::RecordOwnIntent(
        std::int32_t slot, std::span<const std::uint8_t> intentBytes)
    {
        if (_writer == nullptr && !DemoClip::Active())
        {
            return;
        }

        std::vector<std::uint8_t> buffer(2 + intentBytes.size());
        buffer[0] = static_cast<std::uint8_t>(PacketType::SlotIntent);
        buffer[1] = static_cast<std::uint8_t>(slot);
        std::copy(intentBytes.begin(), intentBytes.end(), buffer.begin() + 2);

        const std::span<const std::uint8_t> data(buffer);
        DemoClip::Add(data);
        if (_writer != nullptr)
        {
            const std::uint32_t frame = Frame();
            _writer->WriteRecord(frame, data);
        }
    }

    void DemoRecorder::RecordOwnSnapshot(std::span<const std::uint8_t> payload)
    {
        if (_writer == nullptr && !DemoClip::Active())
        {
            return;
        }

        std::vector<std::uint8_t> buffer(1 + payload.size());
        buffer[0] = static_cast<std::uint8_t>(PacketType::Snapshot);
        std::copy(payload.begin(), payload.end(), buffer.begin() + 1);

        const std::span<const std::uint8_t> data(buffer);
        DemoClip::Add(data);
        if (_writer != nullptr)
        {
            const std::uint32_t frame = Frame();
            _writer->WriteRecord(frame, data);
        }
    }

    std::uint32_t DemoRecorder::Frame()
    {
        const std::uint32_t now = NetSession::NetFrame();
        return now > _startFrame ? now - _startFrame : std::uint32_t{0};
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
