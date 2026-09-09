#include "Mods/Network/net_protocol.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace fruityprime::net {
namespace {

void put_u16(std::span<std::uint8_t> destination, std::size_t offset,
             std::uint16_t value) {
    destination[offset] = static_cast<std::uint8_t>(value & 0xffu);
    destination[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}

[[nodiscard]] std::uint16_t get_u16(std::span<const std::uint8_t> source,
                                    std::size_t offset) {
    return static_cast<std::uint16_t>(source[offset])
        | static_cast<std::uint16_t>(source[offset + 1]) << 8;
}

void put_i16(std::span<std::uint8_t> destination, std::size_t offset,
             std::int16_t value) {
    put_u16(destination, offset, static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t get_i16(std::span<const std::uint8_t> source,
                                   std::size_t offset) {
    return static_cast<std::int16_t>(get_u16(source, offset));
}

void put_u32(std::span<std::uint8_t> destination, std::size_t offset,
             std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        destination[offset + i] = static_cast<std::uint8_t>((value >> (8 * i))
                                                             & 0xffu);
    }
}

[[nodiscard]] std::uint32_t get_u32(std::span<const std::uint8_t> source,
                                    std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(source[offset + i]) << (8 * i);
    }
    return value;
}

void put_f32(std::span<std::uint8_t> destination, std::size_t offset,
             float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(destination, offset, bits);
}

[[nodiscard]] float get_f32(std::span<const std::uint8_t> source,
                            std::size_t offset) {
    const std::uint32_t bits = get_u32(source, offset);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void put_vec3(std::span<std::uint8_t> destination, std::size_t offset,
              const Vec3& value) {
    put_f32(destination, offset, value.x);
    put_f32(destination, offset + 4, value.y);
    put_f32(destination, offset + 8, value.z);
}

[[nodiscard]] Vec3 get_vec3(std::span<const std::uint8_t> source,
                            std::size_t offset) {
    return Vec3{get_f32(source, offset), get_f32(source, offset + 4),
                get_f32(source, offset + 8)};
}

void write_raw(std::span<std::uint8_t> destination, std::string_view value) {
    std::fill(destination.begin(), destination.end(), std::uint8_t{0});
    const std::size_t count = std::min(destination.size(), value.size());
    for (std::size_t i = 0; i < count; ++i) {
        destination[i] = static_cast<std::uint8_t>(value[i]);
    }
}

[[nodiscard]] std::string read_raw(std::span<const std::uint8_t> source) {
    std::size_t length = 0;
    while (length < source.size() && source[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(source.data()), length);
}

void write_ascii(std::span<std::uint8_t> destination, std::string_view value) {
    std::fill(destination.begin(), destination.end(), std::uint8_t{0});
    const std::size_t count = std::min(destination.size(), value.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        destination[i] = c < 32 || c > 126 ? static_cast<std::uint8_t>('?') : c;
    }
}

[[nodiscard]] std::string read_ascii(std::span<const std::uint8_t> source) {
    std::size_t length = 0;
    while (length < source.size() && source[length] != 0) {
        ++length;
    }
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        const auto c = source[i];
        result.push_back(static_cast<char>(c < 32 || c > 126 ? '?' : c));
    }
    return result;
}

void write_match_name(std::span<std::uint8_t> destination,
                      std::string_view value) {
    write_raw(destination.first(MatchStatePacket::MaxNameBytes), value);
}

[[nodiscard]] std::string read_match_name(std::span<const std::uint8_t> source) {
    return read_raw(source.first(MatchStatePacket::MaxNameBytes));
}

} // namespace

void NetText::write(std::span<std::uint8_t> destination, std::string_view value) {
    write_ascii(destination, value);
}

std::string NetText::read(std::span<const std::uint8_t> source) {
    return read_ascii(source);
}

std::array<std::uint8_t, RefusedPacket::Size> RefusedPacket::encode() const {
    return {reason, players, max_players};
}

std::optional<RefusedPacket> RefusedPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    return RefusedPacket{source[0], source[1], source[2]};
}

std::array<std::uint8_t, HostRequestPacket::Size>
HostRequestPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = protocol;
    result[1] = max_players;
    result[2] = mode;
    put_u16(result, 3, time_limit);
    put_u16(result, 5, point_goal);
    NetText::write(std::span(result).subspan(7, MaxRoomBytes), room_key);
    NetText::write(std::span(result).subspan(7 + MaxRoomBytes, MaxNameBytes),
                   server_name);
    return result;
}

std::optional<HostRequestPacket> HostRequestPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    HostRequestPacket result;
    result.protocol = source[0];
    result.max_players = source[1];
    result.mode = source[2];
    result.time_limit = get_u16(source, 3);
    result.point_goal = get_u16(source, 5);
    result.room_key = NetText::read(source.subspan(7, MaxRoomBytes));
    result.server_name = NetText::read(
        source.subspan(7 + MaxRoomBytes, MaxNameBytes));
    return result;
}

std::array<std::uint8_t, HostReplyPacket::Size> HostReplyPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = started ? 1 : 0;
    put_u16(result, 1, port);
    NetText::write(std::span(result).subspan(3, MaxReasonBytes), reason);
    return result;
}

std::optional<HostReplyPacket> HostReplyPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    HostReplyPacket result;
    result.started = source[0] != 0;
    result.port = get_u16(source, 1);
    result.reason = NetText::read(source.subspan(3, MaxReasonBytes));
    return result;
}

std::array<std::uint8_t, MatchStatePacket::Size> MatchStatePacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = mode;
    put_f32(result, 1, time_remaining);
    put_f32(result, 5, time_elapsed);
    result[9] = player_count;
    result[10] = flags;
    put_u16(result, 11, point_goal);
    put_u16(result, 13, match_id);
    write_match_name(std::span(result).subspan(15, MaxNameBytes), room_key);
    write_match_name(std::span(result).subspan(15 + MaxNameBytes, MaxNameBytes),
                     next_room_key);
    return result;
}

std::optional<MatchStatePacket> MatchStatePacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    MatchStatePacket result;
    result.mode = source[0];
    result.time_remaining = get_f32(source, 1);
    result.time_elapsed = get_f32(source, 5);
    result.player_count = source[9];
    result.flags = source[10];
    result.point_goal = get_u16(source, 11);
    result.match_id = get_u16(source, 13);
    result.room_key = read_match_name(source.subspan(15, MaxNameBytes));
    result.next_room_key = read_match_name(
        source.subspan(15 + MaxNameBytes, MaxNameBytes));
    return result;
}

std::array<std::uint8_t, ServerStatusPacket::Size> ServerStatusPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    const auto match_bytes = match.encode();
    std::copy(match_bytes.begin(), match_bytes.end(), result.begin());
    result[MatchStatePacket::Size] = max_players;
    result[MatchStatePacket::Size + 1] = protocol;
    NetText::write(std::span(result).subspan(MatchStatePacket::Size + 2,
                                              MaxNameBytes), server_name);
    return result;
}

std::optional<ServerStatusPacket> ServerStatusPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    const auto match = MatchStatePacket::decode(source.first(MatchStatePacket::Size));
    if (!match) {
        return std::nullopt;
    }
    ServerStatusPacket result;
    result.match = *match;
    result.max_players = source[MatchStatePacket::Size];
    result.protocol = source[MatchStatePacket::Size + 1];
    result.server_name = NetText::read(source.subspan(MatchStatePacket::Size + 2,
                                                       MaxNameBytes));
    return result;
}

std::array<std::uint8_t, MasterEntryPacket::Size> MasterEntryPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    // The C# implementation writes the address in network order, which is the
    // byte order already carried by sockaddr_in::sin_addr.s_addr.
    result[0] = static_cast<std::uint8_t>((address >> 24) & 0xffu);
    result[1] = static_cast<std::uint8_t>((address >> 16) & 0xffu);
    result[2] = static_cast<std::uint8_t>((address >> 8) & 0xffu);
    result[3] = static_cast<std::uint8_t>(address & 0xffu);
    put_u16(result, 4, port);
    result[6] = players;
    result[7] = max_players;
    result[8] = mode;
    result[9] = protocol;
    NetText::write(std::span(result).subspan(10, MaxNameBytes), server_name);
    NetText::write(std::span(result).subspan(10 + MaxNameBytes, MaxRoomBytes),
                   room_key);
    return result;
}

std::optional<MasterEntryPacket> MasterEntryPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    MasterEntryPacket result;
    result.address = static_cast<std::uint32_t>(source[0]) << 24
        | static_cast<std::uint32_t>(source[1]) << 16
        | static_cast<std::uint32_t>(source[2]) << 8
        | static_cast<std::uint32_t>(source[3]);
    result.port = get_u16(source, 4);
    result.players = source[6];
    result.max_players = source[7];
    result.mode = source[8];
    result.protocol = source[9];
    result.server_name = NetText::read(source.subspan(10, MaxNameBytes));
    result.room_key = NetText::read(source.subspan(10 + MaxNameBytes, MaxRoomBytes));
    return result;
}

std::array<std::uint8_t, MasterHeartbeatPacket::Size>
MasterHeartbeatPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = protocol;
    put_u16(result, 1, port);
    result[3] = players;
    result[4] = max_players;
    result[5] = mode;
    NetText::write(std::span(result).subspan(6, MasterEntryPacket::MaxNameBytes),
                   server_name);
    NetText::write(std::span(result).subspan(6 + MasterEntryPacket::MaxNameBytes,
                                              MasterEntryPacket::MaxRoomBytes),
                   room_key);
    return result;
}

std::optional<MasterHeartbeatPacket> MasterHeartbeatPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    MasterHeartbeatPacket result;
    result.protocol = source[0];
    result.port = get_u16(source, 1);
    result.players = source[3];
    result.max_players = source[4];
    result.mode = source[5];
    result.server_name = NetText::read(
        source.subspan(6, MasterEntryPacket::MaxNameBytes));
    result.room_key = NetText::read(source.subspan(
        6 + MasterEntryPacket::MaxNameBytes, MasterEntryPacket::MaxRoomBytes));
    return result;
}

std::array<std::uint8_t, RosterPacket::Size> RosterPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = static_cast<std::uint8_t>(std::min<std::size_t>(count, MaxSlots));
    std::size_t offset = 1;
    for (std::size_t i = 0; i < result[0]; ++i) {
        result[offset] = slots[i];
        result[offset + 1] = hunters[i];
        put_u16(result, offset + 2, pings[i]);
        NetText::write(std::span(result).subspan(offset + 4, MaxNameBytes), names[i]);
        offset += EntrySize;
    }
    return result;
}

std::optional<RosterPacket> RosterPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    RosterPacket result;
    result.count = static_cast<std::uint8_t>(std::min<std::size_t>(source[0], MaxSlots));
    std::size_t offset = 1;
    for (std::size_t i = 0; i < result.count; ++i) {
        result.slots[i] = source[offset];
        result.hunters[i] = source[offset + 1];
        result.pings[i] = get_u16(source, offset + 2);
        result.names[i] = NetText::read(source.subspan(offset + 4, MaxNameBytes));
        offset += EntrySize;
    }
    return result;
}

std::array<std::uint8_t, ChatPacket::Size> ChatPacket::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = slot;
    result[1] = kind;
    write_ascii(std::span(result).subspan(2, MaxNameBytes), name);
    write_ascii(std::span(result).subspan(2 + MaxNameBytes, MaxTextBytes), text);
    return result;
}

std::optional<ChatPacket> ChatPacket::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    return ChatPacket{
        source[0], source[1],
        read_ascii(source.subspan(2, MaxNameBytes)),
        read_ascii(source.subspan(2 + MaxNameBytes, MaxTextBytes))
    };
}

std::optional<std::uint32_t> IntentPacket::frame(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    return get_u32(source, 0);
}

std::array<std::uint8_t, IntentState::Size> IntentState::encode() const {
    std::array<std::uint8_t, Size> result{};
    put_u32(result, 0, frame);
    put_u32(result, 4, static_cast<std::uint32_t>(buttons));
    put_vec3(result, 8, aim);
    result[20] = weapon_select;
    for (std::size_t i = 0; i < PressHistory; ++i) {
        put_u32(result, 21 + i * 4, presses[i]);
    }
    constexpr std::size_t position_offset = 21 + PressHistory * 4;
    put_vec3(result, position_offset, position);
    put_u16(result, position_offset + 12, ammo_ua);
    put_u16(result, position_offset + 14, ammo_missiles);
    return result;
}

std::optional<IntentState> IntentState::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    IntentState result;
    result.frame = get_u32(source, 0);
    result.buttons = static_cast<IntentButtons>(get_u32(source, 4));
    result.aim = get_vec3(source, 8);
    result.weapon_select = source[20];
    for (std::size_t i = 0; i < PressHistory; ++i) {
        result.presses[i] = get_u32(source, 21 + i * 4);
    }
    constexpr std::size_t position_offset = 21 + PressHistory * 4;
    result.position = get_vec3(source, position_offset);
    result.ammo_ua = get_u16(source, position_offset + 12);
    result.ammo_missiles = get_u16(source, position_offset + 14);
    return result;
}

std::array<std::uint8_t, SnapshotHeader::Size> SnapshotHeader::encode() const {
    std::array<std::uint8_t, Size> result{};
    put_u32(result, 0, frame);
    put_u32(result, 4, rng1);
    put_u32(result, 8, rng2);
    result[12] = player_count;
    return result;
}

std::optional<SnapshotHeader> SnapshotHeader::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    return SnapshotHeader{get_u32(source, 0), get_u32(source, 4),
                          get_u32(source, 8), source[12]};
}

std::array<std::uint8_t, PlayerState::Size> PlayerState::encode() const {
    std::array<std::uint8_t, Size> result{};
    result[0] = slot_index;
    result[1] = flags;
    put_vec3(result, 2, position);
    put_vec3(result, 14, speed);
    put_vec3(result, 26, facing);
    put_u16(result, 38, health);
    result[40] = current_weapon;
    result[41] = team;
    result[42] = damage_sequence;
    result[43] = attacker_slot;
    result[44] = damage_beam;
    result[45] = damage_flags;
    put_vec3(result, 46, hit_direction);
    put_i16(result, 58, points);
    put_u16(result, 60, kills);
    put_u16(result, 62, deaths);
    return result;
}

std::optional<PlayerState> PlayerState::decode(
    std::span<const std::uint8_t> source) {
    if (source.size() < Size) {
        return std::nullopt;
    }
    PlayerState result;
    result.slot_index = source[0];
    result.flags = source[1];
    result.position = get_vec3(source, 2);
    result.speed = get_vec3(source, 14);
    result.facing = get_vec3(source, 26);
    result.health = get_u16(source, 38);
    result.current_weapon = source[40];
    result.team = source[41];
    result.damage_sequence = source[42];
    result.attacker_slot = source[43];
    result.damage_beam = source[44];
    result.damage_flags = source[45];
    result.hit_direction = get_vec3(source, 46);
    result.points = get_i16(source, 58);
    result.kills = get_u16(source, 60);
    result.deaths = get_u16(source, 62);
    return result;
}

std::vector<std::uint8_t> SnapshotPacket::encode() const {
    if (players.size() > NetConfig::SlotCapacity) {
        throw std::length_error("snapshot contains too many players");
    }
    const std::size_t size = SnapshotHeader::Size
        + players.size() * PlayerState::Size;
    if (size > NetConfig::MaxPacketSize - 1) {
        throw std::length_error("snapshot exceeds the network packet limit");
    }
    std::vector<std::uint8_t> result(size);
    SnapshotHeader snapshot_header = header;
    snapshot_header.player_count = static_cast<std::uint8_t>(players.size());
    const auto header_bytes = snapshot_header.encode();
    std::copy(header_bytes.begin(), header_bytes.end(), result.begin());
    std::size_t offset = SnapshotHeader::Size;
    for (const PlayerState& player : players) {
        const auto player_bytes = player.encode();
        std::copy(player_bytes.begin(), player_bytes.end(),
                  result.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += PlayerState::Size;
    }
    return result;
}

std::optional<SnapshotPacket> SnapshotPacket::decode(
    std::span<const std::uint8_t> source) {
    const auto header = SnapshotHeader::decode(source);
    if (!header || header->player_count > NetConfig::SlotCapacity) {
        return std::nullopt;
    }
    const std::size_t required = SnapshotHeader::Size
        + static_cast<std::size_t>(header->player_count) * PlayerState::Size;
    if (source.size() < required) {
        return std::nullopt;
    }
    SnapshotPacket result;
    result.header = *header;
    result.players.reserve(header->player_count);
    std::size_t offset = SnapshotHeader::Size;
    for (std::size_t i = 0; i < header->player_count; ++i) {
        const auto player = PlayerState::decode(
            source.subspan(offset, PlayerState::Size));
        if (!player) {
            return std::nullopt;
        }
        result.players.push_back(*player);
        offset += PlayerState::Size;
    }
    return result;
}

std::vector<std::uint8_t> make_datagram(PacketType type,
                                        std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> result;
    result.reserve(payload.size() + 1);
    result.push_back(static_cast<std::uint8_t>(type));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

} // namespace fruityprime::net
