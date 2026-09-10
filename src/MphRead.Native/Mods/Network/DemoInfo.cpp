#include "Formats/demo_info.hpp"

#include "Mods/Network/net_protocol.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <string_view>

namespace fruityprime::demo {
namespace {

[[nodiscard]] std::string packet_type_name(net::PacketType type) {
    using net::PacketType;
    switch (type) {
    case PacketType::Hello: return "Hello";
    case PacketType::Welcome: return "Welcome";
    case PacketType::Intent: return "Intent";
    case PacketType::Snapshot: return "Snapshot";
    case PacketType::Bye: return "Bye";
    case PacketType::Ping: return "Ping";
    case PacketType::Pong: return "Pong";
    case PacketType::MatchState: return "MatchState";
    case PacketType::MapChange: return "MapChange";
    case PacketType::Roster: return "Roster";
    case PacketType::Identify: return "Identify";
    case PacketType::Authority: return "Authority";
    case PacketType::SlotIntent: return "SlotIntent";
    case PacketType::StatusQuery: return "StatusQuery";
    case PacketType::StatusReply: return "StatusReply";
    case PacketType::MatchEnd: return "MatchEnd";
    case PacketType::MasterHeartbeat: return "MasterHeartbeat";
    case PacketType::MasterQuery: return "MasterQuery";
    case PacketType::MasterList: return "MasterList";
    case PacketType::HostRequest: return "HostRequest";
    case PacketType::HostReply: return "HostReply";
    case PacketType::Refused: return "Refused";
    case PacketType::Chat: return "Chat";
    }
    return std::to_string(static_cast<unsigned int>(type));
}

} // namespace

std::optional<Info> inspect(const std::filesystem::path& path) {
    auto reader = Reader::open(path);
    if (!reader.has_value()) {
        return std::nullopt;
    }
    Info info;
    info.protocol = reader->protocol_version();
    std::uint32_t previous_frame = 0;
    while (const auto record = reader->read_next()) {
        if (!info.have_frame) {
            info.first_frame = record->frame;
            previous_frame = record->frame;
            info.have_frame = true;
        }
        info.biggest_gap = std::max(
            info.biggest_gap, record->frame - previous_frame);
        previous_frame = record->frame;
        info.last_frame = record->frame;
        info.payload_bytes += record->data.size();
        ++info.records;
        if (!record->data.empty()) {
            const std::size_t type = record->data.front();
            if (info.packet_counts[type] == 0) {
                info.packet_order.push_back(static_cast<std::uint8_t>(type));
            }
            ++info.packet_counts[type];
            info.packet_bytes[type] += record->data.size();
            if (type == static_cast<std::uint8_t>(net::PacketType::Snapshot)) {
                ++info.snapshots;
            }
        }
    }
    info.inflated_bytes = reader->decompressed_size();
    info.tail_truncated = reader->had_deflate_error();
    std::error_code size_error;
    info.on_disk = std::filesystem::file_size(path, size_error);
    return info;
}

void print(std::ostream& output, const std::filesystem::path& path,
           const Info& info) {
    const std::uint32_t frame_count = !info.have_frame
        ? 0 : info.last_frame - info.first_frame + 1;
    const double seconds = static_cast<double>(frame_count) / 60.0;
    output << "[demo] " << path.string() << '\n'
           << "  protocol " << static_cast<int>(info.protocol)
           << " (this build: "
           << static_cast<int>(net::NetConfig::ProtocolVersion) << ')';
    if (info.protocol != net::NetConfig::ProtocolVersion) {
        output << "  -- MISMATCH";
    }
    output << '\n'
           << "  " << info.records << " record(s) over frames "
           << (info.have_frame ? info.first_frame : 0) << '-'
           << (info.have_frame ? info.last_frame : 0) << " ("
           << std::fixed << std::setprecision(1) << seconds
           << " s at 60 fps)\n"
           << "  " << std::setprecision(1)
           << (static_cast<double>(info.on_disk) / 1024.0)
           << " KiB on disk, "
           << (static_cast<double>(info.payload_bytes) / 1024.0)
           << " KiB of packets -- "
           << std::setprecision(2)
           << (info.payload_bytes > 0
                   ? static_cast<double>(info.payload_bytes)
                         / static_cast<double>(info.on_disk)
                   : 0.0)
           << "x, " << std::setprecision(1)
           << (seconds > 0.0
                   ? static_cast<double>(info.on_disk) / seconds / 1024.0
                   : 0.0)
           << " KiB/s\n"
           << "  longest gap between records: " << info.biggest_gap
           << " frame(s)\n";
    for (const std::uint8_t value : info.packet_order) {
        const auto type = static_cast<net::PacketType>(value);
        output << "  " << std::left << std::setw(14)
               << packet_type_name(type) << std::right << std::setw(8)
               << info.packet_counts[value] << " ("
               << std::setw(6)
               << (seconds > 0.0
                       ? static_cast<double>(info.packet_counts[value])
                             / seconds
                       : 0.0)
               << "/s, " << std::setprecision(1)
               << (static_cast<double>(info.packet_bytes[value]) / 1024.0)
               << " KiB)\n";
    }
}

int print_command(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        std::cout << "[demo] no such file: " << path.string() << '\n';
        return 1;
    }

    const auto info = inspect(path);
    if (!info.has_value()) {
        std::cout << "[demo] \"" << path.string()
                  << "\" is not a demo this build can read (bad magic, or "
                     "not format version "
                  << static_cast<int>(FormatVersion) << ")\n";
        return 1;
    }
    print(std::cout, path, *info);
    if (info->snapshots == 0) {
        std::cout << "  NO SNAPSHOTS -- nothing in this file ever places a player, "
                     "so it will play back as an empty room.\n";
        return 1;
    }
    return 0;
}

} // namespace fruityprime::demo
