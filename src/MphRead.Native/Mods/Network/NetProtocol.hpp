#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MphRead::Mods::Network
{
    enum class PacketType : std::uint8_t
    {
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
        Chat = 23,
        Vote = 26,
        VoteState = 27
    };

    struct RefusedPacket
    {
        static constexpr std::int32_t Size = 3;
        static constexpr std::uint8_t ReasonFull = 1;
        static constexpr std::uint8_t ReasonProtocol = 2;

        std::uint8_t Reason = 0;
        std::uint8_t Players = 0;
        std::uint8_t MaxPlayers = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static RefusedPacket Read(std::span<const std::uint8_t> src);
        [[nodiscard]] std::string Describe(const std::optional<std::string>& where) const;
    };

    struct HostRequestPacket
    {
        static constexpr std::int32_t MaxRoomBytes = 40;
        static constexpr std::int32_t MaxNameBytes = 32;
        static constexpr std::int32_t Size = 1 + 1 + 1 + 2 + 2 + MaxRoomBytes + MaxNameBytes;

        std::uint8_t Protocol = 0;
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Mode = 0;
        std::uint16_t TimeLimit = 0;
        std::uint16_t PointGoal = 0;
        std::optional<std::string> RoomKey{};
        std::optional<std::string> ServerName{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static HostRequestPacket Read(std::span<const std::uint8_t> src);
    };

    struct HostReplyPacket
    {
        static constexpr std::int32_t MaxReasonBytes = 96;
        static constexpr std::int32_t Size = 1 + 2 + MaxReasonBytes;

        bool Started = false;
        std::uint16_t Port = 0;
        std::optional<std::string> Reason{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static HostReplyPacket Read(std::span<const std::uint8_t> src);
    };

    class NetText final
    {
    public:
        static void Write(std::span<std::uint8_t> dest, const std::optional<std::string>& value);
        [[nodiscard]] static std::string Read(std::span<const std::uint8_t> src);

        NetText() = delete;
    };

    struct MasterEntryPacket
    {
        static constexpr std::int32_t MaxNameBytes = 32;
        static constexpr std::int32_t MaxRoomBytes = 40;
        static constexpr std::int32_t Size = 4 + 2 + 1 + 1 + 1 + 1 + MaxNameBytes + MaxRoomBytes;

        std::uint32_t Address = 0;
        std::uint16_t Port = 0;
        std::uint8_t Players = 0;
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Mode = 0;
        std::uint8_t Protocol = 0;
        std::optional<std::string> ServerName{};
        std::optional<std::string> RoomKey{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static MasterEntryPacket Read(std::span<const std::uint8_t> src);
    };

    struct MasterHeartbeatPacket
    {
        static constexpr std::int32_t Size
            = 1 + 2 + 1 + 1 + 1 + MasterEntryPacket::MaxNameBytes + MasterEntryPacket::MaxRoomBytes;

        std::uint8_t Protocol = 0;
        std::uint16_t Port = 0;
        std::uint8_t Players = 0;
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Mode = 0;
        std::optional<std::string> ServerName{};
        std::optional<std::string> RoomKey{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static MasterHeartbeatPacket Read(std::span<const std::uint8_t> src);
    };

    struct MatchStatePacket
    {
        static constexpr std::int32_t MaxNameBytes = 40;
        static constexpr std::int32_t Size = 1 + 4 + 4 + 1 + 1 + 2 + 2 + MaxNameBytes + MaxNameBytes;

        std::uint8_t Mode = 0;
        float TimeRemaining = 0.0F;
        float TimeElapsed = 0.0F;
        std::uint8_t PlayerCount = 0;
        std::uint8_t Flags = 0;
        std::uint16_t PointGoal = 0;
        std::uint16_t MatchId = 0;
        std::optional<std::string> RoomKey{};
        std::optional<std::string> NextRoomKey{};

        static constexpr std::uint8_t FlagInProgress = 1U << 0;
        static constexpr std::uint8_t FlagEnding = 1U << 1;
        static constexpr std::uint8_t FlagFriendlyFire = 1U << 2;
        static constexpr std::uint8_t FlagNoShadowFreeze = 1U << 3;

        [[nodiscard]] bool Ending() const noexcept;
        [[nodiscard]] bool FriendlyFire() const noexcept;
        [[nodiscard]] bool ShadowFreeze() const noexcept;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static MatchStatePacket Read(std::span<const std::uint8_t> src);

    private:
        static void WriteName(std::span<std::uint8_t> dest,
            const std::optional<std::string>& value);
        [[nodiscard]] static std::string ReadName(std::span<const std::uint8_t> src);
    };

    struct ServerStatusPacket
    {
        static constexpr std::int32_t MaxNameBytes = 32;
        static constexpr std::int32_t Size = MatchStatePacket::Size + 2 + MaxNameBytes;

        MatchStatePacket Match{};
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Protocol = 0;
        std::optional<std::string> ServerName{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static ServerStatusPacket Read(std::span<const std::uint8_t> src);
    };

    enum class IntentButtons : std::uint32_t
    {
        None = 0,
        MoveLeft = 1U << 0,
        MoveRight = 1U << 1,
        MoveUp = 1U << 2,
        MoveDown = 1U << 3,
        Shoot = 1U << 4,
        Zoom = 1U << 5,
        Jump = 1U << 6,
        Morph = 1U << 7,
        Boost = 1U << 8,
        AltAttack = 1U << 9,
        ScanVisor = 1U << 10,
        NextWeapon = 1U << 11,
        PrevWeapon = 1U << 12,
        RollLeft = 1U << 13,
        RollRight = 1U << 14,
        RollUp = 1U << 15,
        RollDown = 1U << 16,
        ZoomedState = 1U << 17,
        AltFormState = 1U << 18,
        InPlayState = 1U << 19,
        SpectatingState = 1U << 20,
        ReadyState = 1U << 21
    };

    [[nodiscard]] constexpr IntentButtons operator|(IntentButtons left, IntentButtons right) noexcept
    {
        return static_cast<IntentButtons>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr IntentButtons operator&(IntentButtons left, IntentButtons right) noexcept
    {
        return static_cast<IntentButtons>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr IntentButtons operator^(IntentButtons left, IntentButtons right) noexcept
    {
        return static_cast<IntentButtons>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr IntentButtons operator~(IntentButtons value) noexcept
    {
        return static_cast<IntentButtons>(~static_cast<std::uint32_t>(value));
    }

    constexpr IntentButtons& operator|=(IntentButtons& left, IntentButtons right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr IntentButtons& operator&=(IntentButtons& left, IntentButtons right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr IntentButtons& operator^=(IntentButtons& left, IntentButtons right) noexcept
    {
        left = left ^ right;
        return left;
    }

    struct RosterPacket
    {
        static constexpr std::int32_t MaxNameBytes = 16;
        static constexpr std::int32_t MaxSlots = 8;
        static constexpr std::int32_t EntrySize = 1 + 1 + 1 + 2 + MaxNameBytes;
        static constexpr std::int32_t Size = 1 + MaxSlots * EntrySize;

        std::uint8_t Count = 0;
        std::shared_ptr<std::vector<std::uint8_t>> Slots{};
        std::shared_ptr<std::vector<std::uint8_t>> Hunters{};
        std::shared_ptr<std::vector<std::uint8_t>> Colors{};
        std::shared_ptr<std::vector<std::uint16_t>> Pings{};
        std::shared_ptr<std::vector<std::optional<std::string>>> Names{};

        [[nodiscard]] static RosterPacket Create();
        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static RosterPacket Read(std::span<const std::uint8_t> src);

    private:
        static void WriteName(std::span<std::uint8_t> dest,
            const std::optional<std::string>& value);
        [[nodiscard]] static std::string ReadName(std::span<const std::uint8_t> src);
    };

    struct ChatPacket
    {
        static constexpr std::int32_t MaxNameBytes = 16;
        static constexpr std::int32_t MaxTextBytes = 96;
        static constexpr std::int32_t Size = 1 + 1 + MaxNameBytes + MaxTextBytes;

        static constexpr std::uint8_t KindSay = 0;
        static constexpr std::uint8_t KindTeam = 1;
        static constexpr std::uint8_t KindSystem = 2;

        std::uint8_t Slot = 0;
        std::uint8_t Kind = 0;
        std::optional<std::string> Name{};
        std::optional<std::string> Text{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static ChatPacket Read(std::span<const std::uint8_t> src);

        static void WriteAscii(std::span<std::uint8_t> dest,
            const std::optional<std::string>& value);
        [[nodiscard]] static std::string ReadAscii(std::span<const std::uint8_t> src);
    };

    struct IntentPacket
    {
        static constexpr std::int32_t PressHistory = 8;
        static constexpr std::int32_t Size = 4 + 4 + 12 + 1 + 4 * PressHistory + 12 + 2 + 2 + 4;

        std::uint32_t Frame = 0;
        IntentButtons Buttons = IntentButtons::None;
        std::shared_ptr<std::vector<std::uint32_t>> Presses{};
        ::OpenTK::Mathematics::Vector3 Aim{};
        ::OpenTK::Mathematics::Vector3 Position{};
        std::uint8_t WeaponSelect = 0;
        std::uint16_t AmmoUa = 0;
        std::uint16_t AmmoMissiles = 0;
        std::uint32_t AckFrame = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static IntentPacket Read(std::span<const std::uint8_t> src);
    };

    struct PlayerState
    {
        static constexpr std::int32_t Size
            = 1 + 1 + 12 + 12 + 12 + 2 + 1 + 1 + 1 + 1 + 1 + 1 + 12 + 2 + 2 + 2;

        std::uint8_t SlotIndex = 0;
        std::uint8_t Flags = 0;
        ::OpenTK::Mathematics::Vector3 Position{};
        ::OpenTK::Mathematics::Vector3 Speed{};
        ::OpenTK::Mathematics::Vector3 Facing{};
        std::uint16_t Health = 0;
        std::uint8_t CurrentWeapon = 0;
        std::uint8_t Team = 0;
        std::uint8_t DamageSeq = 0;
        std::uint8_t AttackerSlot = 0;
        std::uint8_t DamageBeam = 0;
        std::uint8_t DamageFlags = 0;
        ::OpenTK::Mathematics::Vector3 HitDirection{};
        std::int16_t Points = 0;
        std::uint16_t Kills = 0;
        std::uint16_t Deaths = 0;

        static constexpr std::uint8_t FlagActive = 1U << 0;
        static constexpr std::uint8_t FlagAltForm = 1U << 1;
        static constexpr std::uint8_t FlagSpawned = 1U << 2;
        static constexpr std::uint8_t FlagZoomed = 1U << 3;
        static constexpr std::uint8_t FlagSpectating = 1U << 4;
        static constexpr std::uint8_t FlagFrozen = 1U << 5;
        static constexpr std::uint8_t FlagDisrupted = 1U << 6;
        static constexpr std::uint8_t FlagBurning = 1U << 7;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static PlayerState Read(std::span<const std::uint8_t> src);

    private:
        static void WriteVec(std::span<std::uint8_t> dest,
            ::OpenTK::Mathematics::Vector3 value);
        [[nodiscard]] static ::OpenTK::Mathematics::Vector3 ReadVec(
            std::span<const std::uint8_t> src);
    };

    struct SnapshotHeader
    {
        static constexpr std::int32_t Size = 4 + 4 + 4 + 1;

        std::uint32_t Frame = 0;
        std::uint32_t Rng1 = 0;
        std::uint32_t Rng2 = 0;
        std::uint8_t PlayerCount = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static SnapshotHeader Read(std::span<const std::uint8_t> src);
    };

    class NetConfig final
    {
    public:
        static constexpr std::uint16_t DefaultPort = 27888;
        static constexpr std::int32_t MaxPacketSize = 1024;
        static constexpr std::int32_t ProtocolVersion = 6;
        static constexpr std::int32_t IntentSendInterval = 1;
        static constexpr double TimeoutSeconds = 30.0;

        NetConfig() = delete;
    };

    struct VotePacket
    {
        static constexpr std::int32_t MaxRoomBytes = MatchStatePacket::MaxNameBytes;
        static constexpr std::int32_t Size = 1 + MaxRoomBytes;

        static constexpr std::uint8_t KindPropose = 0;
        static constexpr std::uint8_t KindYes = 1;
        static constexpr std::uint8_t KindNo = 2;

        std::uint8_t Kind = 0;
        std::optional<std::string> RoomKey{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static VotePacket Read(std::span<const std::uint8_t> src);
    };

    struct VoteStatePacket
    {
        static constexpr std::int32_t MaxRoomBytes = MatchStatePacket::MaxNameBytes;
        static constexpr std::int32_t MaxNameBytes = ChatPacket::MaxNameBytes;
        static constexpr std::int32_t Size = 1 + MaxRoomBytes + MaxNameBytes + 1 + 1 + 1 + 1 + 2;

        static constexpr std::uint8_t StateIdle = 0;
        static constexpr std::uint8_t StateRunning = 1;
        static constexpr std::uint8_t StatePassed = 2;
        static constexpr std::uint8_t StateFailed = 3;

        std::uint8_t State = 0;
        std::optional<std::string> RoomKey{};
        std::optional<std::string> Proposer{};
        std::uint8_t Yes = 0;
        std::uint8_t No = 0;
        std::uint8_t Eligible = 0;
        std::uint8_t Needed = 0;
        std::uint16_t Seconds = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static VoteStatePacket Read(std::span<const std::uint8_t> src);
    };
}
