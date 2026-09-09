#include "Mods/Network/demo_playback.hpp"

#include <span>

namespace fruityprime::demo {

bool Playback::open(const std::filesystem::path& path,
                    net::MatchStatePacket& first_match,
                    std::string& error) {
    stop();
    auto reader = Reader::open(path);
    if (!reader.has_value()) {
        error = "replay file is not a readable FPDM demo: " + path.string();
        return false;
    }

    std::optional<net::MatchStatePacket> match;
    bool has_snapshot = false;
    std::vector<Record> records;
    while (auto record = reader->read_next()) {
        if (!record->data.empty()) {
            const std::span<const std::uint8_t> packet(record->data);
            const auto type = static_cast<net::PacketType>(packet.front());
            if (type == net::PacketType::MatchState && !match.has_value()) {
                match = net::MatchStatePacket::decode(packet.subspan(1));
            } else if (type == net::PacketType::Snapshot) {
                has_snapshot = net::SnapshotPacket::decode(packet.subspan(1))
                    .has_value() || has_snapshot;
            }
        }
        records.push_back(std::move(*record));
    }
    if (!match.has_value() || match->room_key.empty()) {
        error = "replay file has no MatchState room to load: "
            + path.string();
        return false;
    }
    if (!has_snapshot) {
        error = "replay file has no valid Snapshot records: " + path.string();
        return false;
    }

    protocol_mismatch_ = reader->protocol_version()
        != net::NetConfig::ProtocolVersion;
    first_match = *match;
    records_ = std::move(records);
    cursor_ = 0;
    active_ = true;
    return true;
}

void Playback::stop() noexcept {
    records_.clear();
    cursor_ = 0;
    active_ = false;
    protocol_mismatch_ = false;
}

std::vector<Record> Playback::take_until(std::uint32_t frame) {
    std::vector<Record> result;
    if (!active_) {
        return result;
    }
    while (cursor_ < records_.size() && records_[cursor_].frame <= frame) {
        result.push_back(std::move(records_[cursor_]));
        ++cursor_;
    }
    return result;
}

} // namespace fruityprime::demo
