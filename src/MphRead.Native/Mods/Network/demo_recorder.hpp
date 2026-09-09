#pragma once

#include "Mods/Network/demo.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::demo {

// The runtime counterpart of Mods/Network/DemoRecorder.cs.  The game owns
// the network session and calls this small module from its receive/send
// paths; the module owns the writer and the frame origin so a recording is
// always stamped from frame zero, even when recording starts mid-match.
class Recorder final {
public:
    Recorder() = default;
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    Recorder(Recorder&&) = delete;
    Recorder& operator=(Recorder&&) = delete;
    ~Recorder();

    [[nodiscard]] bool start(const std::filesystem::path& path,
                             std::uint32_t current_frame);
    void stop() noexcept;

    [[nodiscard]] bool recording() const noexcept {
        return writer_ != nullptr;
    }
    [[nodiscard]] const std::filesystem::path& current_path() const noexcept {
        return path_;
    }

    void record_bytes(std::uint32_t current_frame,
                      std::span<const std::uint8_t> data);
    void record_packet(std::uint32_t current_frame, net::PacketType type,
                       std::span<const std::uint8_t> payload);

    // Matches DemoRecorder.Start's ROOM_yyyy-MM-dd_HH-mm-ss.fpdemo naming.
    [[nodiscard]] static std::filesystem::path default_path(
        const std::filesystem::path& export_root, std::string_view room,
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] static std::string sanitize_file_name(
        std::string_view name);

private:
    std::unique_ptr<Writer> writer_;
    std::filesystem::path path_;
    std::uint32_t start_frame_ = 0;
};

} // namespace fruityprime::demo
