#include "Mods/Network/demo_recorder.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fruityprime::demo {
namespace {

[[nodiscard]] std::tm local_time(std::chrono::system_clock::time_point now) {
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm result{};
#if defined(_WIN32)
    if (localtime_s(&result, &value) != 0) {
        throw std::runtime_error("could not convert demo timestamp");
    }
#else
    if (const std::tm* converted = std::localtime(&value); converted != nullptr) {
        result = *converted;
    } else {
        throw std::runtime_error("could not convert demo timestamp");
    }
#endif
    return result;
}

} // namespace

Recorder::~Recorder() {
    stop();
}

bool Recorder::start(const std::filesystem::path& path,
                     std::uint32_t current_frame) {
    if (recording()) {
        return false;
    }
    try {
        writer_ = std::make_unique<Writer>(path);
    } catch (const std::exception&) {
        writer_.reset();
        path_.clear();
        return false;
    }
    path_ = path;
    start_frame_ = current_frame;
    return true;
}

void Recorder::stop() noexcept {
    if (writer_ != nullptr) {
        try {
            writer_->close();
        } catch (...) {
            // A disk disappearing while the game is closing must not turn a
            // normal window close into process termination.
        }
        writer_.reset();
    }
    path_.clear();
    start_frame_ = 0;
}

void Recorder::record_bytes(std::uint32_t current_frame,
                            std::span<const std::uint8_t> data) {
    if (writer_ == nullptr) {
        return;
    }
    const std::uint32_t frame = current_frame >= start_frame_
        ? current_frame - start_frame_ : 0;
    writer_->write_record(frame, data);
}

void Recorder::record_packet(std::uint32_t current_frame, net::PacketType type,
                             std::span<const std::uint8_t> payload) {
    if (writer_ == nullptr) {
        return;
    }
    std::vector<std::uint8_t> packet;
    packet.push_back(static_cast<std::uint8_t>(type));
    packet.insert(packet.end(), payload.begin(), payload.end());
    record_bytes(current_frame, packet);
}

std::string Recorder::sanitize_file_name(std::string_view name) {
    std::string result(name);
    // This is the Windows invalid filename set used by the game build.  The
    // control range is included explicitly because it is not visible in a
    // room key but is still rejected by CreateFileW.
    constexpr std::string_view invalid = "<>:\"/\\|?*";
    for (char& value : result) {
        const unsigned char byte = static_cast<unsigned char>(value);
        if (byte < 0x20 || invalid.find(value) != std::string_view::npos) {
            value = '_';
        }
    }
    return result;
}

std::filesystem::path Recorder::default_path(
    const std::filesystem::path& export_root, std::string_view room,
    std::chrono::system_clock::time_point now) {
    const std::tm stamp = local_time(now);
    std::ostringstream name;
    name << sanitize_file_name(room) << '_' << std::setfill('0')
         << std::setw(4) << stamp.tm_year + 1900 << '-'
         << std::setw(2) << stamp.tm_mon + 1 << '-'
         << std::setw(2) << stamp.tm_mday << '_'
         << std::setw(2) << stamp.tm_hour << '-'
         << std::setw(2) << stamp.tm_min << '-'
         << std::setw(2) << stamp.tm_sec << Extension;
    return export_root / "_demos" / name.str();
}

} // namespace fruityprime::demo
