#pragma once

#include "MatchDefinition.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Guid.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
        VoteState = 27,
        MapChoices = 28,
        MapPick = 29,
        HitClaim = 30,
        HitVerdict = 31,
        // Map transfer is negotiated before loading a custom room. All
        // requests are bounded and identify package hashes rather than peer
        // filenames.
        MapOffer = 32,
        MapWant = 33,
        MapChunk = 34,
        SessionState = 36,
        LobbyCommand = 37,
        LobbyCommandResult = 38,
        MatchLoaded = 39,
        MatchLoadFailed = 40,
        MapDone = 35
    };

    struct RefusedPacket
    {
        static constexpr std::int32_t Size = 3;
        static constexpr std::uint8_t ReasonFull = 1;
        static constexpr std::uint8_t ReasonProtocol = 2;
        static constexpr std::uint8_t ReasonKicked = 3;
        static constexpr std::uint8_t ReasonInMatch = 4;

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

        // How many maps a requested rotation may carry, and what one costs on
        // the wire. The cap is the datagram rather than a policy.
        static constexpr std::int32_t MaxRotation = 16;
        static constexpr std::int32_t RotationEntrySize = MaxRoomBytes + 1;

        std::uint8_t Protocol = 0;
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Mode = 0;
        std::uint16_t TimeLimit = 0;
        std::uint16_t PointGoal = 0;
        // The C# parameterless constructor sets both to "" and the two flags
        // to true; `default` (ZeroInitialized) leaves them null and false.
        std::optional<std::string> RoomKey = std::string();
        std::optional<std::string> ServerName = std::string();

        // Every map the asker wants played, in order, or nothing. Written
        // *after* the fixed block, so a directory from before rotations reads
        // exactly Size bytes and plays RoomKey on a loop.
        std::optional<std::vector<std::pair<std::string, ::MphRead::GameMode>>> Rotation{};

        ServerSessionPolicy Policy = ServerSessionPolicy::Continuous;
        bool AllowJoinInProgress = true;
        bool RequireReady = true;
        MatchFormat Format = MatchFormat::Auto;

        // How many bytes this request takes, tail included.
        [[nodiscard]] std::int32_t Length() const noexcept;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static HostRequestPacket Read(std::span<const std::uint8_t> src);
        // default(HostRequestPacket): every field zero, bypassing the
        // constructor's defaults.
        [[nodiscard]] static HostRequestPacket ZeroInitialized();

    private:
        [[nodiscard]] static std::optional<std::vector<std::pair<std::string, ::MphRead::GameMode>>>
            ReadRotation(std::span<const std::uint8_t> src);
    };

    struct HostReplyPacket
    {
        static constexpr std::int32_t MaxReasonBytes = 96;
        static constexpr std::int32_t Size = 1 + 2 + MaxReasonBytes + 16;

        ::MphRead::NativeRuntime::Guid OwnerToken{};
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
        std::uint64_t AuthorityEpoch = 0;
        static constexpr std::int32_t MaxNameBytes = 40;
        static constexpr std::int32_t Size = 1 + 4 + 4 + 1 + 1 + 2 + 2 + MaxNameBytes + MaxNameBytes + 8;

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
        // Bits 4-5: the damage level every machine in this match scales its
        // hits by, as the level plus one, so that zero means "this server did
        // not say".
        static constexpr std::uint8_t FlagDamageShift = 4;
        static constexpr std::uint8_t FlagDamageMask = 0b11U << FlagDamageShift;
        // Bit 6: weapon pickups are the picking hunter's affinity variant.
        static constexpr std::uint8_t FlagAffinityWeapons = 1U << 6;

        [[nodiscard]] bool Ending() const noexcept;
        [[nodiscard]] bool FriendlyFire() const noexcept;
        [[nodiscard]] bool ShadowFreeze() const noexcept;
        // The damage level this server plays at, or -1 when it did not say.
        [[nodiscard]] std::int32_t DamageLevel() const noexcept;
        // Whether this server states its damage rules at all.
        [[nodiscard]] bool StatesRules() const noexcept;
        [[nodiscard]] bool AffinityWeapons() const noexcept;
        // Pack the two rules into the spare bits of the flags byte. A level
        // outside 0-2 is "do not say", which is what an older server sends.
        [[nodiscard]] static std::uint8_t RuleFlags(
            std::int32_t damageLevel, bool affinityWeapons) noexcept;

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
        // The same packet with five bytes of capability and session on the
        // end, after the name so that the length check separates old from new.
        static constexpr std::int32_t SizeWithFlags = Size + 5;
        // Bit 0: this server will open a new match on a port of its own.
        static constexpr std::uint8_t FlagCanHost = 1;

        SessionPhase Phase = SessionPhase::Lobby;
        MatchFormat Format = MatchFormat::Auto;
        bool LobbyEnabled = false;
        bool AllowJoinInProgress = false;
        MatchStatePacket Match{};
        std::uint8_t MaxPlayers = 0;
        std::uint8_t Protocol = 0;
        // What this server can do beyond running the match it is running.
        // Zero for a server that did not say, which reads as "cannot".
        std::uint8_t Flags = 0;
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
        static constexpr std::int32_t EntrySize = 1 + 1 + 1 + 2 + MaxNameBytes + 4;
        static constexpr std::int32_t HeaderSize = 17;
        static constexpr std::int32_t Size = HeaderSize + MaxSlots * EntrySize;
        std::uint16_t SessionRevision = 0;
        std::uint16_t MatchId = 0;
        std::uint64_t AuthorityEpoch = 0;
        std::uint32_t Revision = 0;
        std::shared_ptr<std::vector<std::uint16_t>> Generations{};

        std::uint8_t Count = 0;
        std::shared_ptr<std::vector<std::int8_t>> Teams{};
        std::shared_ptr<std::vector<bool>> LobbyReady{};
        std::shared_ptr<std::vector<std::uint8_t>> Slots{};
        std::shared_ptr<std::vector<std::uint8_t>> Hunters{};
        std::shared_ptr<std::vector<std::uint8_t>> Colors{};
        std::shared_ptr<std::vector<std::uint16_t>> Pings{};
        std::shared_ptr<std::vector<std::optional<std::string>>> Names{};

        [[nodiscard]] static RosterPacket Create();
        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static bool TryRead(std::span<const std::uint8_t> src, RosterPacket& roster);
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
        std::uint16_t MatchId = 0;
        std::uint64_t AuthorityEpoch = 0;
        std::uint16_t SlotGeneration = 0;
        std::uint16_t LifeId = 0;
        static constexpr std::int32_t PressHistory = 8;
        static constexpr std::int32_t Size = 4 + 4 + 12 + 1 + 4 * PressHistory + 12 + 2 + 2 + 4 + 1 + 14;

        // Four bytes appended past Size, carrying the state that decides what
        // this player's next shot is worth: the charge, the alt-form ram's
        // strength and the two multipliers. Appended rather than folded in, so
        // a build from before finds none and behaves as it always did.
        static constexpr std::int32_t StateSize = 4;
        static constexpr std::int32_t FullSize = Size + StateSize;

        std::uint8_t ChargeLevel = 0;
        std::uint8_t BoostDamage = 0;
        std::uint8_t ShotFlags = 0;

        static constexpr std::uint8_t FlagDoubleDamage = 1U << 0;
        // Sent but not applied: who the Prime Hunter is is the authority's
        // own state. It travels so that a mismatch shows up in a log.
        static constexpr std::uint8_t FlagPrimeHunter = 1U << 1;

        // Whether the sender included the block at all.
        bool HasState = false;

        std::uint32_t Frame = 0;
        IntentButtons Buttons = IntentButtons::None;
        std::shared_ptr<std::vector<std::uint32_t>> Presses{};
        ::OpenTK::Mathematics::Vector3 Aim{};
        ::OpenTK::Mathematics::Vector3 Position{};
        std::uint8_t WeaponSelect = 0;
        std::uint16_t AmmoUa = 0;
        std::uint16_t AmmoMissiles = 0;
        std::uint32_t AckFrame = 0;
        // How far past AckFrame the world this client was looking at actually
        // sat, in 1/256ths of a frame. Zero from a client that does not
        // interpolate.
        std::uint8_t AckSubFrame = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static IntentPacket Read(std::span<const std::uint8_t> src);
    };

    struct DamageEvent
    {
        static constexpr std::int32_t Size = 15;

        std::uint16_t EventId = 0;
        std::uint16_t AttackerGeneration = 0;
        std::uint16_t Damage = 0;
        std::uint8_t AttackerSlot = 0;
        std::uint8_t Beam = 0;
        std::uint8_t Flags = 0;
        ::OpenTK::Mathematics::Vector3 Direction{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static DamageEvent Read(std::span<const std::uint8_t> src);

    private:
        static constexpr float DirectionScale = 16384.0F;
        [[nodiscard]] static std::int16_t PackDirection(float value) noexcept;
        [[nodiscard]] static float UnpackDirection(std::int16_t value) noexcept;
    };

    struct PlayerState
    {
        std::uint16_t SlotGeneration = 0;
        std::uint16_t LifeId = 0;
        static constexpr std::int32_t DamageHistory = 4;
        static constexpr std::int32_t Size = 54 + DamageEvent::Size * DamageHistory;

        std::uint8_t SlotIndex = 0;
        std::uint8_t Flags = 0;
        ::OpenTK::Mathematics::Vector3 Position{};
        ::OpenTK::Mathematics::Vector3 Speed{};
        ::OpenTK::Mathematics::Vector3 Facing{};
        std::uint16_t Health = 0;
        std::uint8_t CurrentWeapon = 0;
        std::uint8_t Team = 0;
        std::uint16_t DamageEventId = 0;
        DamageEvent Damage0{};
        DamageEvent Damage1{};
        DamageEvent Damage2{};
        DamageEvent Damage3{};
        [[nodiscard]] DamageEvent EventAt(std::int32_t index) const;
        // Derived on Read from the newest damage event.
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
        std::uint16_t MatchId = 0;
        std::uint64_t AuthorityEpoch = 0;
        static constexpr std::int32_t Size = 4 + 4 + 4 + 1 + 10;

        std::uint32_t Frame = 0;
        std::uint32_t Rng1 = 0;
        std::uint32_t Rng2 = 0;
        std::uint8_t PlayerCount = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static SnapshotHeader Read(std::span<const std::uint8_t> src);
    };

    struct HitClaimPacket
    {
        std::uint16_t MatchId = 0;
        std::uint64_t AuthorityEpoch = 0;
        std::uint16_t ShooterGeneration = 0;
        std::uint16_t ShooterLifeId = 0;
        std::uint16_t VictimGeneration = 0;
        std::uint16_t VictimLifeId = 0;
        static constexpr std::int32_t Size = 2 + 4 + 4 + 4 + 1 + 1 + 2 + 1 + 12 + 18;

        static constexpr std::int32_t MaxPerPacket = 6;

        static constexpr std::uint8_t NoBeam = 0xFF;

        static constexpr std::uint8_t FlagHeadshot = 1U << 0;
        static constexpr std::uint8_t FlagLethal = 1U << 1;
        static constexpr std::uint8_t FlagFrozen = 1U << 2;
        static constexpr std::uint8_t FlagBurning = 1U << 3;
        static constexpr std::uint8_t FlagDisrupted = 1U << 4;

        std::uint16_t ClaimId = 0;
        std::uint32_t Frame = 0;
        std::uint32_t AckFrame = 0;
        std::uint32_t LaunchFrame = 0;
        std::uint8_t VictimSlot = 0;
        std::uint8_t Beam = 0;
        std::uint16_t Damage = 0;
        std::uint8_t Flags = 0;
        ::OpenTK::Mathematics::Vector3 HitPoint{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static HitClaimPacket Read(std::span<const std::uint8_t> src);
    };

    struct HitVerdictPacket
    {
        static constexpr std::int32_t HeaderSize = 15;
        static constexpr std::int32_t EntrySize = 3;
        static constexpr std::int32_t MaxPerPacket = 16;

        static constexpr std::uint8_t ResultApplied = 0;
        static constexpr std::uint8_t ResultDuplicate = 1;
        static constexpr std::uint8_t ResultDeadShooter = 2;
        static constexpr std::uint8_t ResultDeadVictim = 3;
        static constexpr std::uint8_t ResultRefused = 4;
        static constexpr std::uint8_t ResultTooOld = 5;
        static constexpr std::uint8_t ResultWrongLife = 6;
        static constexpr std::uint8_t ResultGeometry = 7;
        static constexpr std::uint8_t ResultDamageLimit = 8;
        static constexpr std::uint8_t ResultInvalidLaunch = 9;
        static constexpr std::uint8_t ResultNoDamage = 10;

        std::uint16_t ClaimId = 0;
        std::uint8_t Result = 0;

        static void Write(std::span<std::uint8_t> dest,
            std::span<const std::pair<std::uint16_t, std::uint8_t>> entries,
            std::uint16_t matchId, std::uint64_t epoch, std::uint16_t generation,
            std::uint16_t lifeId);
        [[nodiscard]] static std::string Describe(std::uint8_t result);
    };

    class NetConfig final
    {
    public:
        static constexpr std::uint16_t DefaultPort = 27888;
        static constexpr std::int32_t MaxPacketSize = 1232;
        static constexpr std::int32_t ProtocolVersion = 14;
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

    struct MapChoicesPacket
    {
        static constexpr std::int32_t MaxChoices = 8;
        static constexpr std::int32_t MaxRoomBytes = MatchStatePacket::MaxNameBytes;
        static constexpr std::int32_t Size = 4 + MaxChoices * (MaxRoomBytes + 1);

        std::uint8_t Open = 0;

        std::uint8_t Count = 0;
        std::shared_ptr<std::vector<std::optional<std::string>>> RoomKeys{};
        std::shared_ptr<std::vector<std::uint8_t>> Votes{};
        std::uint8_t Eligible = 0;

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static MapChoicesPacket Read(std::span<const std::uint8_t> src);
    };

    struct MapPickPacket
    {
        static constexpr std::int32_t MaxRoomBytes = MatchStatePacket::MaxNameBytes;
        static constexpr std::int32_t Size = MaxRoomBytes;

        std::optional<std::string> RoomKey{};

        void Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static MapPickPacket Read(std::span<const std::uint8_t> src);
    };
}
