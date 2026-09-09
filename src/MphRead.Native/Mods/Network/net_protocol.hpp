#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::net {

enum class PacketType : std::uint8_t {
    Hello = 1,
    Welcome = 2,
    Intent = 3,
    Snapshot = 4,
    Bye = 5,
    Ping = 6,
    Pong = 7,
    MatchState = 8,
    MapChange = 9,
    Roster = 10,
    Identify = 11,
    Authority = 12,
    SlotIntent = 13,
    StatusQuery = 14,
    StatusReply = 15,
    MatchEnd = 16,
    MasterHeartbeat = 17,
    MasterQuery = 18,
    MasterList = 19,
    HostRequest = 20,
    HostReply = 21,
    Refused = 22,
    Chat = 23
};

struct NetConfig {
    static constexpr std::uint16_t DefaultPort = 27888;
    static constexpr std::size_t MaxPacketSize = 1024;
    static constexpr std::uint8_t ProtocolVersion = 4;
    static constexpr double TimeoutSeconds = 30.0;
    static constexpr std::uint32_t IntentResetGap = 600;
    static constexpr std::uint32_t IntentSendInterval = 1;
    static constexpr std::size_t SlotCapacity = 8;
};

struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;
};

class NetText {
public:
    static void write(std::span<std::uint8_t> destination, std::string_view value);
    static std::string read(std::span<const std::uint8_t> source);
};

struct RefusedPacket {
    static constexpr std::size_t Size = 3;
    static constexpr std::uint8_t ReasonFull = 1;
    static constexpr std::uint8_t ReasonProtocol = 2;

    std::uint8_t reason = 0;
    std::uint8_t players = 0;
    std::uint8_t max_players = 0;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<RefusedPacket> decode(
        std::span<const std::uint8_t> source);
};

struct HostRequestPacket {
    static constexpr std::size_t MaxRoomBytes = 40;
    static constexpr std::size_t MaxNameBytes = 32;
    static constexpr std::size_t Size = 1 + 1 + 1 + 2 + 2
        + MaxRoomBytes + MaxNameBytes;

    std::uint8_t protocol = NetConfig::ProtocolVersion;
    std::uint8_t max_players = 4;
    std::uint8_t mode = 3; // GameMode.Battle
    std::uint16_t time_limit = 0;
    std::uint16_t point_goal = 7;
    std::string room_key;
    std::string server_name;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<HostRequestPacket> decode(
        std::span<const std::uint8_t> source);
};

struct HostReplyPacket {
    static constexpr std::size_t MaxReasonBytes = 96;
    static constexpr std::size_t Size = 1 + 2 + MaxReasonBytes;

    bool started = false;
    std::uint16_t port = 0;
    std::string reason;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<HostReplyPacket> decode(
        std::span<const std::uint8_t> source);
};

struct MatchStatePacket {
    static constexpr std::size_t MaxNameBytes = 40;
    static constexpr std::size_t Size = 1 + 4 + 4 + 1 + 1 + 2 + 2
        + MaxNameBytes + MaxNameBytes;
    static constexpr std::uint8_t FlagInProgress = 1u << 0;
    static constexpr std::uint8_t FlagEnding = 1u << 1;
    static constexpr std::uint8_t FlagFriendlyFire = 1u << 2;

    std::uint8_t mode = 3; // GameMode.Battle
    float time_remaining = 0;
    float time_elapsed = 0;
    std::uint8_t player_count = 0;
    std::uint8_t flags = FlagInProgress;
    std::uint16_t point_goal = 7;
    std::uint16_t match_id = 1;
    std::string room_key;
    std::string next_room_key;

    [[nodiscard]] bool ending() const { return (flags & FlagEnding) != 0; }
    [[nodiscard]] bool friendly_fire() const {
        return (flags & FlagFriendlyFire) != 0;
    }

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<MatchStatePacket> decode(
        std::span<const std::uint8_t> source);
};

struct ServerStatusPacket {
    static constexpr std::size_t MaxNameBytes = 32;
    static constexpr std::size_t Size = MatchStatePacket::Size + 2 + MaxNameBytes;

    MatchStatePacket match;
    std::uint8_t max_players = 0;
    std::uint8_t protocol = NetConfig::ProtocolVersion;
    std::string server_name;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<ServerStatusPacket> decode(
        std::span<const std::uint8_t> source);
};

struct MasterEntryPacket {
    static constexpr std::size_t MaxNameBytes = 32;
    static constexpr std::size_t MaxRoomBytes = 40;
    static constexpr std::size_t Size = 4 + 2 + 1 + 1 + 1 + 1
        + MaxNameBytes + MaxRoomBytes;

    std::uint32_t address = 0; // IPv4 in network byte order.
    std::uint16_t port = 0;
    std::uint8_t players = 0;
    std::uint8_t max_players = 0;
    std::uint8_t mode = 0;
    std::uint8_t protocol = NetConfig::ProtocolVersion;
    std::string server_name;
    std::string room_key;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<MasterEntryPacket> decode(
        std::span<const std::uint8_t> source);
};

struct MasterHeartbeatPacket {
    static constexpr std::size_t Size = 1 + 2 + 1 + 1 + 1
        + MasterEntryPacket::MaxNameBytes + MasterEntryPacket::MaxRoomBytes;

    std::uint8_t protocol = NetConfig::ProtocolVersion;
    std::uint16_t port = 0;
    std::uint8_t players = 0;
    std::uint8_t max_players = 0;
    std::uint8_t mode = 0;
    std::string server_name;
    std::string room_key;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<MasterHeartbeatPacket> decode(
        std::span<const std::uint8_t> source);
};

struct RosterPacket {
    static constexpr std::size_t MaxNameBytes = 16;
    static constexpr std::size_t MaxSlots = NetConfig::SlotCapacity;
    static constexpr std::size_t EntrySize = 1 + 1 + 2 + MaxNameBytes;
    static constexpr std::size_t Size = 1 + MaxSlots * EntrySize;

    std::uint8_t count = 0;
    std::array<std::uint8_t, MaxSlots> slots{};
    std::array<std::uint8_t, MaxSlots> hunters{};
    std::array<std::uint16_t, MaxSlots> pings{};
    std::array<std::string, MaxSlots> names{};

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<RosterPacket> decode(
        std::span<const std::uint8_t> source);
};

struct ChatPacket {
    static constexpr std::size_t MaxNameBytes = 16;
    static constexpr std::size_t MaxTextBytes = 96;
    static constexpr std::size_t Size = 1 + 1 + MaxNameBytes + MaxTextBytes;
    static constexpr std::uint8_t KindSay = 0;
    static constexpr std::uint8_t KindTeam = 1;
    static constexpr std::uint8_t KindSystem = 2;

    std::uint8_t slot = 0xff;
    std::uint8_t kind = KindSay;
    std::string name;
    std::string text;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<ChatPacket> decode(
        std::span<const std::uint8_t> source);
};

struct IntentPacket {
    static constexpr std::size_t PressHistory = 8;
    static constexpr std::size_t Size = 4 + 4 + 12 + 1 + 4 * PressHistory
        + 12 + 2 + 2;

    [[nodiscard]] static std::optional<std::uint32_t> frame(
        std::span<const std::uint8_t> source);
};

enum class IntentButtons : std::uint32_t {
    None = 0,
    MoveLeft = 1u << 0,
    MoveRight = 1u << 1,
    MoveUp = 1u << 2,
    MoveDown = 1u << 3,
    Shoot = 1u << 4,
    Zoom = 1u << 5,
    Jump = 1u << 6,
    Morph = 1u << 7,
    Boost = 1u << 8,
    AltAttack = 1u << 9,
    ScanVisor = 1u << 10,
    NextWeapon = 1u << 11,
    PrevWeapon = 1u << 12,
    RollLeft = 1u << 13,
    RollRight = 1u << 14,
    RollUp = 1u << 15,
    RollDown = 1u << 16,
    ZoomedState = 1u << 17,
    AltFormState = 1u << 18,
    InPlayState = 1u << 19,
    SpectatingState = 1u << 20
};

struct IntentState {
    static constexpr std::size_t PressHistory = IntentPacket::PressHistory;
    static constexpr std::size_t Size = IntentPacket::Size;

    std::uint32_t frame = 0;
    IntentButtons buttons = IntentButtons::None;
    Vec3 aim;
    std::uint8_t weapon_select = 0xff;
    std::array<std::uint32_t, PressHistory> presses{};
    Vec3 position;
    std::uint16_t ammo_ua = 0;
    std::uint16_t ammo_missiles = 0;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<IntentState> decode(
        std::span<const std::uint8_t> source);
};

struct SnapshotHeader {
    static constexpr std::size_t Size = 4 + 4 + 4 + 1;

    std::uint32_t frame = 0;
    std::uint32_t rng1 = 0;
    std::uint32_t rng2 = 0;
    std::uint8_t player_count = 0;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<SnapshotHeader> decode(
        std::span<const std::uint8_t> source);
};

struct PlayerState {
    static constexpr std::size_t Size = 64;
    static constexpr std::uint8_t FlagActive = 1u << 0;
    static constexpr std::uint8_t FlagAltForm = 1u << 1;
    static constexpr std::uint8_t FlagSpawned = 1u << 2;
    static constexpr std::uint8_t FlagZoomed = 1u << 3;
    static constexpr std::uint8_t FlagSpectating = 1u << 4;
    static constexpr std::uint8_t FlagFrozen = 1u << 5;

    std::uint8_t slot_index = 0;
    std::uint8_t flags = 0;
    Vec3 position;
    Vec3 speed;
    Vec3 facing;
    std::uint16_t health = 0;
    std::uint8_t current_weapon = 0;
    std::uint8_t team = 0;
    std::uint8_t damage_sequence = 0;
    std::uint8_t attacker_slot = 0xff;
    std::uint8_t damage_beam = 0xff;
    std::uint8_t damage_flags = 0;
    Vec3 hit_direction;
    std::int16_t points = 0;
    std::uint16_t kills = 0;
    std::uint16_t deaths = 0;

    [[nodiscard]] std::array<std::uint8_t, Size> encode() const;
    [[nodiscard]] static std::optional<PlayerState> decode(
        std::span<const std::uint8_t> source);
};

struct SnapshotPacket {
    SnapshotHeader header;
    std::vector<PlayerState> players;

    [[nodiscard]] std::vector<std::uint8_t> encode() const;
    [[nodiscard]] static std::optional<SnapshotPacket> decode(
        std::span<const std::uint8_t> source);
};

[[nodiscard]] std::vector<std::uint8_t> make_datagram(
    PacketType type, std::span<const std::uint8_t> payload);

} // namespace fruityprime::net
