#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace MphRead::Mods::Network
{
    class DemoWriter;
    struct ReceivedPacket;

    class DemoRecorder final
    {
    public:
        DemoRecorder() = delete;
        DemoRecorder(const DemoRecorder&) = delete;
        DemoRecorder(DemoRecorder&&) = delete;
        DemoRecorder& operator=(const DemoRecorder&) = delete;
        DemoRecorder& operator=(DemoRecorder&&) = delete;

        [[nodiscard]] static bool IsRecording() noexcept;
        [[nodiscard]] static std::optional<std::string> CurrentPath();

        [[nodiscard]] static bool Start();
        static void Stop();

        static void Record(ReceivedPacket packet);
        static void RecordOwnIntent(std::int32_t slot, std::span<const std::uint8_t> intentBytes);
        static void RecordOwnSnapshot(std::span<const std::uint8_t> payload);

    private:
        [[nodiscard]] static std::uint32_t Frame();
        [[nodiscard]] static std::string SanitizeFileName(std::string name);

        static std::unique_ptr<DemoWriter> _writer;
        static std::uint32_t _startFrame;
        static std::optional<std::string> _currentPath;
    };
}
