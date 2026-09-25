#include "NetHitClaims.hpp"

#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "NetLifecycleTracker.hpp"
#include "NetLog.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetRoomChange.hpp"
#include "NetSession.hpp"
#include "NetSmoothing.hpp"
#include "NetUnlagged.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../NativeRuntime/System/BinaryPrimitives.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::Mods::Network
{
    namespace
    {
        namespace Runtime = ::MphRead::NativeRuntime;
        using ::MphRead::Entities::DamageFlags;
        using ::MphRead::Entities::LoadFlags;
        using ::MphRead::Entities::PlayerEntity;

        [[nodiscard]] bool HasDamageFlag(DamageFlags value, DamageFlags flag) noexcept
        {
            return (static_cast<std::int32_t>(value) & static_cast<std::int32_t>(flag))
                == static_cast<std::int32_t>(flag);
        }

        [[nodiscard]] bool HasLoadFlag(LoadFlags value, LoadFlags flag) noexcept
        {
            using U = std::underlying_type_t<LoadFlags>;
            return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
        }

        [[nodiscard]] std::int32_t Ping(std::int32_t slot)
        {
            return slot >= 0 && slot < static_cast<std::int32_t>(NetSession::SlotPing.size())
                ? NetSession::SlotPing[static_cast<std::size_t>(slot)]
                : 0;
        }

        [[nodiscard]] PlayerEntity& PlayerAt(std::int32_t slot)
        {
            return Runtime::RequireReference(Runtime::ManagedAt(PlayerEntity::Players(), slot));
        }
    }

    bool NetHitClaims::_enabled = true;
    std::array<NetHitClaims::Outgoing, NetHitClaims::OutboxCapacity> NetHitClaims::_outbox{};
    std::uint16_t NetHitClaims::_nextId = 1;
    std::int64_t NetHitClaims::_declared = 0;
    std::int64_t NetHitClaims::_applied = 0;
    std::int64_t NetHitClaims::_duplicate = 0;
    std::int64_t NetHitClaims::_refusedDeadShooter = 0;
    std::int64_t NetHitClaims::_refusedDeadVictim = 0;
    std::int64_t NetHitClaims::_refusedOther = 0;
    std::int64_t NetHitClaims::_unanswered = 0;
    std::int64_t NetHitClaims::_resends = 0;
    std::array<NetHitClaims::Pending, NetHitClaims::PendingCapacity> NetHitClaims::_pending{};
    std::array<std::uint16_t, NetHitClaims::Slots> NetHitClaims::_newestId{};
    std::array<std::uint8_t, NetHitClaims::Slots> NetHitClaims::_lastResult{};
    NetHitClaims::Grid<std::uint16_t, NetHitClaims::Slots, NetHitClaims::SeenCapacity> NetHitClaims::_seenIds{};
    NetHitClaims::Grid<std::uint8_t, NetHitClaims::Slots, NetHitClaims::SeenCapacity> NetHitClaims::_seenResults{};
    NetHitClaims::Grid<::MphRead::BeamType, NetHitClaims::Slots, NetHitClaims::SeenCapacity> NetHitClaims::_seenBeams{};
    NetHitClaims::Grid<ShotKey, NetHitClaims::Slots, NetHitClaims::SeenCapacity> NetHitClaims::_seenKeys{};
    NetHitClaims::Ledger<std::uint32_t> NetHitClaims::_authorityHit{};
    NetHitClaims::Ledger<std::uint32_t> NetHitClaims::_authorityHitAck{};
    NetHitClaims::Ledger<std::uint32_t> NetHitClaims::_authorityHitLaunch{};
    NetHitClaims::Ledger<bool> NetHitClaims::_authorityHitUsed{};
    NetHitClaims::Ledger<std::int32_t> NetHitClaims::_authorityHitDamage{};
    NetHitClaims::Grid<std::int32_t, NetHitClaims::Slots, NetHitClaims::Slots> NetHitClaims::_authorityHitHead{};
    std::array<std::int64_t, NetHitClaims::AltBeamBuckets> NetHitClaims::_agreeByBeam{};
    std::array<std::int64_t, NetHitClaims::AltBeamBuckets> NetHitClaims::_differByBeam{};
    std::array<std::int64_t, NetHitClaims::AltBeamBuckets> NetHitClaims::_claimedByBeam{};
    std::array<std::int64_t, NetHitClaims::AltBeamBuckets> NetHitClaims::_resolvedByBeam{};
    std::int32_t NetHitClaims::_disagreementsLogged = 0;
    std::array<std::uint32_t, NetHitClaims::Slots> NetHitClaims::_deathFire{};
    std::array<bool, NetHitClaims::Slots> NetHitClaims::_dead{};
    std::array<std::uint32_t, NetHitClaims::Slots> NetHitClaims::_lastHitFire{};
    std::array<bool, NetHitClaims::Slots> NetHitClaims::_wasInPlay{};
    bool NetHitClaims::_applyingClaim = false;
    std::uint32_t NetHitClaims::_applyingClaimAck = 0;
    std::uint32_t NetHitClaims::_applyingClaimLaunch = 0;
    std::int64_t NetHitClaims::_received = 0;
    std::int64_t NetHitClaims::_appliedHere = 0;
    std::int64_t NetHitClaims::_duplicateHere = 0;
    std::int64_t NetHitClaims::_voidedDeadShooter = 0;
    std::int64_t NetHitClaims::_voidedDeadVictim = 0;
    std::int64_t NetHitClaims::_refusedHere = 0;
    std::int64_t NetHitClaims::_tooOldHere = 0;
    std::int64_t NetHitClaims::_repeatsHere = 0;
    std::int64_t NetHitClaims::_matchedByLaunch = 0;
    std::int64_t NetHitClaims::_matchedByWindow = 0;
    std::int64_t NetHitClaims::_rescuedDamage = 0;
    std::int64_t NetHitClaims::_rescuedKills = 0;
    std::int64_t NetHitClaims::_rescuedHeadshots = 0;
    std::int64_t NetHitClaims::_suppressedHere = 0;
    std::array<std::uint8_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedAttacker{};
    std::array<std::uint8_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedVictim{};
    std::array<ShotKey, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedKeys{};
    std::array<std::uint16_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedVictimGeneration{};
    std::array<std::uint16_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedVictimLife{};
    std::array<std::uint32_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedAt{};
    std::array<std::int32_t, NetHitClaims::RescuedCapacity> NetHitClaims::_rescuedOwed{};
    std::int32_t NetHitClaims::_rescuedHead = 0;
    NetHitClaims::VerdictWriter NetHitClaims::_verdictSink{};
    NetHitClaims::Grid<std::pair<std::uint16_t, std::uint8_t>, NetHitClaims::Slots, NetHitClaims::VerdictCapacity>
        NetHitClaims::_verdicts{};
    std::array<std::int32_t, NetHitClaims::Slots> NetHitClaims::_verdictCount{};

    std::int32_t NetHitClaims::GraceFor(std::int32_t slot)
    {
        const std::int32_t ping = Ping(slot);
        return Runtime::MathClamp(static_cast<std::int32_t>(static_cast<float>(ping) * 0.06F) + 20,
            MinGraceFrames, MaxGraceFrames);
    }

    std::int32_t NetHitClaims::GraceFrames()
    {
        return GraceFor(NetSession::LocalSlot());
    }

    std::int32_t NetHitClaims::MaxClaimAge()
    {
        return NetUnlagged::HistoryFrames - 4;
    }

    std::int32_t NetHitClaims::ResendInterval()
    {
        const std::int32_t ping = Ping(NetSession::LocalSlot());
        return Runtime::MathClamp(static_cast<std::int32_t>(static_cast<float>(ping) * 0.06F) + 6,
            MinResendInterval, 40);
    }

    bool NetHitClaims::Claiming()
    {
        return _enabled && NetSession::Active() && !NetSession::IsAuthority() && !NetSession::IsHost()
            && NetSession::LocalSlot() >= 0;
    }

    std::uint16_t NetHitClaims::Declare(PlayerEntity& victim, PlayerEntity& attacker, ::MphRead::BeamType beam,
        std::uint32_t damage, DamageFlags flags, bool lethal, OpenTK::Mathematics::Vector3 hitPoint,
        std::uint32_t launchFrame)
    {
        if (!Claiming() || &victim == &attacker || damage == 0
            || NetPlayerLifecycle::Get(victim.SlotIndex()) == 0 || NetPlayerLifecycle::Get(attacker.SlotIndex()) == 0)
        {
            return 0;
        }
        const std::int32_t slot = victim.SlotIndex();
        if (slot < 0 || slot >= Slots)
        {
            return 0;
        }
        std::uint8_t claimFlags = 0;
        if (HasDamageFlag(flags, DamageFlags::Headshot))
        {
            claimFlags = static_cast<std::uint8_t>(claimFlags | HitClaimPacket::FlagHeadshot);
        }
        if (lethal)
        {
            claimFlags = static_cast<std::uint8_t>(claimFlags | HitClaimPacket::FlagLethal);
        }
        if (victim.ModFrozen())
        {
            claimFlags = static_cast<std::uint8_t>(claimFlags | HitClaimPacket::FlagFrozen);
        }
        std::int32_t index = -1;
        for (std::int32_t i = 0; i < OutboxCapacity; i++)
        {
            if (!_outbox[static_cast<std::size_t>(i)].Live)
            {
                index = i;
                break;
            }
        }
        if (index < 0)
        {
            std::int32_t oldest = 0;
            for (std::int32_t i = 1; i < OutboxCapacity; i++)
            {
                if (_outbox[static_cast<std::size_t>(i)].Age > _outbox[static_cast<std::size_t>(oldest)].Age)
                {
                    oldest = i;
                }
            }
            index = oldest;
            _unanswered++;
        }
        std::uint32_t readFrame = 0;
        std::uint8_t subFrame = 0;
        const std::uint32_t ack = NetSmoothing::AckPoint(readFrame, subFrame)
            ? readFrame
            : NetSession::AppliedSnapshotFrame();
        Outgoing entry;
        entry.MatchId = NetSession::CurrentMatchId();
        entry.AuthorityEpoch = NetSession::AuthorityEpoch();
        entry.ShooterGeneration = NetPlayerLifecycle::Generation(attacker.SlotIndex());
        entry.ShooterLifeId = NetPlayerLifecycle::Get(attacker.SlotIndex());
        entry.VictimGeneration = NetPlayerLifecycle::Generation(slot);
        entry.VictimLifeId = NetPlayerLifecycle::Get(slot);
        entry.Id = _nextId;
        entry.Frame = NetSession::NetFrame();
        entry.AckFrame = ack;
        entry.LaunchFrame = launchFrame;
        entry.VictimSlot = static_cast<std::uint8_t>(slot);
        entry.Beam = beam == ::MphRead::BeamType::None ? HitClaimPacket::NoBeam : static_cast<std::uint8_t>(beam);
        entry.Damage = static_cast<std::uint16_t>(std::min<std::uint32_t>(damage, 0xFFFFU));
        entry.Flags = claimFlags;
        entry.HitPoint = hitPoint;
        entry.Age = 0;
        entry.Sends = 0;
        entry.Live = true;
        _outbox[static_cast<std::size_t>(index)] = entry;
        const std::uint16_t id = _nextId;
        _nextId++;
        if (_nextId == 0)
        {
            _nextId = 1;
        }
        _declared++;
        NetShotDiagnostics::Claims[static_cast<std::size_t>(NetShotDiagnostics::Bucket(beam))]++;
        if (NetLog::Enabled())
        {
            NetShotDiagnostics::Trace("claim", ShotKey::For(attacker.SlotIndex(), launchFrame), beam,
                "id=" + std::to_string(id) + " victim=" + std::to_string(slot) + " ack=" + std::to_string(ack)
                + " damage=" + std::to_string(damage));
        }
        return id;
    }

    std::int32_t NetHitClaims::Compose(std::span<std::uint8_t> dest)
    {
        if (!Claiming())
        {
            return 0;
        }
        std::int32_t count = 0;
        std::size_t offset = 1;
        const std::int32_t interval = ResendInterval();
        for (std::size_t i = 0; i < OutboxCapacity && count < HitClaimPacket::MaxPerPacket; i++)
        {
            Outgoing& entry = _outbox[i];
            if (!entry.Live)
            {
                continue;
            }
            if (entry.Sends > 0 && (entry.Sends >= MaxSends || entry.Age % interval != 0))
            {
                continue;
            }
            HitClaimPacket packet;
            packet.MatchId = entry.MatchId;
            packet.AuthorityEpoch = entry.AuthorityEpoch;
            packet.ShooterGeneration = entry.ShooterGeneration;
            packet.ShooterLifeId = entry.ShooterLifeId;
            packet.VictimGeneration = entry.VictimGeneration;
            packet.VictimLifeId = entry.VictimLifeId;
            packet.ClaimId = entry.Id;
            packet.Frame = entry.Frame;
            packet.AckFrame = entry.AckFrame;
            packet.LaunchFrame = entry.LaunchFrame;
            packet.VictimSlot = entry.VictimSlot;
            packet.Beam = entry.Beam;
            packet.Damage = entry.Damage;
            packet.Flags = entry.Flags;
            packet.HitPoint = entry.HitPoint;
            packet.Write(Runtime::SpanSlice(dest, offset));
            offset += HitClaimPacket::Size;
            if (entry.Sends > 0)
            {
                _resends++;
            }
            entry.Sends++;
            count++;
        }
        if (count == 0)
        {
            return 0;
        }
        Runtime::ManagedAt(dest, 0) = static_cast<std::uint8_t>(count);
        return static_cast<std::int32_t>(offset);
    }

    void NetHitClaims::TickOutbox()
    {
        for (Outgoing& entry : _outbox)
        {
            if (!entry.Live)
            {
                continue;
            }
            entry.Age++;
            if (entry.Age > MaxAge)
            {
                entry.Live = false;
                _unanswered++;
                if (NetLog::Enabled())
                {
                    NetShotDiagnostics::Trace("unanswered",
                        ShotKey(entry.AuthorityEpoch, entry.MatchId, NetSession::LocalSlot(),
                            entry.ShooterGeneration, entry.ShooterLifeId, entry.LaunchFrame),
                        static_cast<::MphRead::BeamType>(entry.Beam),
                        "id=" + std::to_string(entry.Id) + " reason=verdict-timeout sends=" + std::to_string(entry.Sends));
                }
                NetHitPrediction::Settle(entry.VictimSlot, entry.Id, false);
            }
        }
    }

    void NetHitClaims::ApplyVerdicts(std::span<const std::uint8_t> payload)
    {
        if (payload.empty())
        {
            return;
        }
        if (payload.size() < static_cast<std::size_t>(HitVerdictPacket::HeaderSize))
        {
            return;
        }
        if (!NetSession::MatchesStream(Runtime::ReadUInt16LittleEndian(Runtime::SpanSlice(payload, 1)),
                Runtime::ReadUInt64LittleEndian(Runtime::SpanSlice(payload, 3)))
            || !NetPlayerLifecycle::Matches(NetSession::LocalSlot(),
                Runtime::ReadUInt16LittleEndian(Runtime::SpanSlice(payload, 11)),
                Runtime::ReadUInt16LittleEndian(Runtime::SpanSlice(payload, 13))))
        {
            return;
        }
        const std::int32_t count = std::min(static_cast<std::int32_t>(payload[0]), HitVerdictPacket::MaxPerPacket);
        for (std::int32_t i = 0; i < count; i++)
        {
            const std::size_t at = static_cast<std::size_t>(HitVerdictPacket::HeaderSize + i * HitVerdictPacket::EntrySize);
            if (at + HitVerdictPacket::EntrySize > payload.size())
            {
                break;
            }
            const auto id = static_cast<std::uint16_t>(payload[at] | (payload[at + 1] << 8));
            const std::uint8_t result = payload[at + 2];
            for (Outgoing& entry : _outbox)
            {
                if (!entry.Live || entry.Id != id
                    || !NetPlayerLifecycle::Matches(entry.VictimSlot, entry.VictimGeneration, entry.VictimLifeId))
                {
                    continue;
                }
                entry.Live = false;
                const auto weapon = static_cast<std::size_t>(
                    NetShotDiagnostics::Bucket(static_cast<::MphRead::BeamType>(entry.Beam)));
                if (result == HitVerdictPacket::ResultApplied)
                {
                    NetShotDiagnostics::Rescues[weapon]++;
                }
                else if (result != HitVerdictPacket::ResultDuplicate)
                {
                    NetShotDiagnostics::Refusals[weapon]++;
                }
                if (NetLog::Enabled())
                {
                    NetShotDiagnostics::Trace("verdict",
                        ShotKey(entry.AuthorityEpoch, entry.MatchId, NetSession::LocalSlot(),
                            entry.ShooterGeneration, entry.ShooterLifeId, entry.LaunchFrame),
                        static_cast<::MphRead::BeamType>(entry.Beam),
                        "id=" + std::to_string(id) + " result=" + HitVerdictPacket::Describe(result));
                }
                switch (result)
                {
                case HitVerdictPacket::ResultApplied:
                    _applied++;
                    NetHitPrediction::Settle(entry.VictimSlot, id, true);
                    break;
                case HitVerdictPacket::ResultDuplicate:
                    _duplicate++;
                    NetHitPrediction::Settle(entry.VictimSlot, id, true);
                    break;
                case HitVerdictPacket::ResultDeadShooter:
                    _refusedDeadShooter++;
                    NetHitPrediction::Settle(entry.VictimSlot, id, false);
                    break;
                case HitVerdictPacket::ResultDeadVictim:
                    _refusedDeadVictim++;
                    NetHitPrediction::Settle(entry.VictimSlot, id, false);
                    break;
                default:
                    _refusedOther++;
                    NetHitPrediction::Settle(entry.VictimSlot, id, false);
                    NetLog::Event("claim " + std::to_string(id) + " on slot " + std::to_string(entry.VictimSlot)
                        + " " + HitVerdictPacket::Describe(result));
                    break;
                }
                break;
            }
        }
    }

    void NetHitClaims::NoteLedger(std::int32_t attacker, std::int32_t victim, std::uint32_t ack,
        std::uint32_t launch, std::int32_t damage, bool used)
    {
        const auto a = static_cast<std::size_t>(attacker);
        const auto v = static_cast<std::size_t>(victim);
        const std::int32_t head = _authorityHitHead[a][v];
        const auto h = static_cast<std::size_t>(head);
        _authorityHit[a][v][h] = NetSession::NetFrame();
        _authorityHitAck[a][v][h] = ack;
        _authorityHitLaunch[a][v][h] = launch;
        _authorityHitDamage[a][v][h] = damage;
        _authorityHitUsed[a][v][h] = used;
        _authorityHitHead[a][v] = (head + 1) % LedgerDepth;
    }

    std::int32_t NetHitClaims::NearestLedgerOffset(std::int32_t attacker, std::int32_t victim, std::uint32_t arrived)
    {
        std::int32_t best = std::numeric_limits<std::int32_t>::min();
        for (std::size_t i = 0; i < LedgerDepth; i++)
        {
            const std::uint32_t at = _authorityHit[static_cast<std::size_t>(attacker)][static_cast<std::size_t>(victim)][i];
            if (at == 0)
            {
                continue;
            }
            const std::int32_t offset = static_cast<std::int32_t>(arrived) - static_cast<std::int32_t>(at);
            if (best == std::numeric_limits<std::int32_t>::min() || std::abs(offset) < std::abs(best))
            {
                best = offset;
            }
        }
        return best;
    }

    bool NetHitClaims::TakeLedger(std::int32_t attacker, std::int32_t victim, std::uint32_t claimAck,
        std::uint32_t claimLaunch, std::uint32_t arrived, std::int32_t window, std::int32_t& authorityDamage)
    {
        const auto a = static_cast<std::size_t>(attacker);
        const auto v = static_cast<std::size_t>(victim);
        authorityDamage = 0;
        if (claimLaunch != 0)
        {
            for (std::size_t i = 0; i < LedgerDepth; i++)
            {
                if (_authorityHitUsed[a][v][i] || _authorityHit[a][v][i] == 0)
                {
                    continue;
                }
                const std::uint32_t launch = _authorityHitLaunch[a][v][i];
                if (launch != 0 && std::llabs(static_cast<std::int64_t>(launch) - claimLaunch) <= LaunchMatchFrames)
                {
                    _authorityHitUsed[a][v][i] = true;
                    authorityDamage = _authorityHitDamage[a][v][i];
                    _matchedByLaunch++;
                    return true;
                }
            }
        }
        const std::uint32_t floor = arrived > static_cast<std::uint32_t>(window)
            ? arrived - static_cast<std::uint32_t>(window) : 0U;
        std::int32_t best = -1;
        std::int64_t bestGap = std::numeric_limits<std::int64_t>::max();
        for (std::size_t i = 0; i < LedgerDepth; i++)
        {
            if (_authorityHitUsed[a][v][i])
            {
                continue;
            }
            const std::uint32_t at = _authorityHit[a][v][i];
            if (at == 0 || at < floor || at > arrived + static_cast<std::uint32_t>(window))
            {
                continue;
            }
            const std::uint32_t launch = _authorityHitLaunch[a][v][i];
            if (launch != 0 && claimLaunch != 0)
            {
                continue;
            }
            const std::int64_t gap = std::llabs(static_cast<std::int64_t>(_authorityHitAck[a][v][i]) - claimAck);
            if (gap <= AckMatchFrames && gap < bestGap)
            {
                bestGap = gap;
                best = static_cast<std::int32_t>(i);
            }
        }
        if (best < 0)
        {
            return false;
        }
        _authorityHitUsed[a][v][static_cast<std::size_t>(best)] = true;
        authorityDamage = _authorityHitDamage[a][v][static_cast<std::size_t>(best)];
        _matchedByWindow++;
        return true;
    }

    void NetHitClaims::ClearLedger(std::int32_t attacker, std::int32_t victim)
    {
        const auto a = static_cast<std::size_t>(attacker);
        const auto v = static_cast<std::size_t>(victim);
        for (std::size_t i = 0; i < LedgerDepth; i++)
        {
            _authorityHit[a][v][i] = 0;
            _authorityHitAck[a][v][i] = 0;
            _authorityHitLaunch[a][v][i] = 0;
            _authorityHitDamage[a][v][i] = 0;
            _authorityHitUsed[a][v][i] = false;
        }
        _authorityHitHead[a][v] = 0;
    }

    std::uint32_t NetHitClaims::CurrentClaimLaunch() noexcept
    {
        return _applyingClaim ? _applyingClaimLaunch : 0U;
    }

    bool NetHitClaims::Arbitrating()
    {
        return _enabled && NetSession::Active()
            && (NetSession::Role() == NetRole::Host || NetSession::IsAuthority());
    }

    void NetHitClaims::NoteAuthorityHit(std::int32_t attackerSlot, std::int32_t victimSlot,
        std::uint32_t launchFrame, std::int32_t damage)
    {
        if (!Arbitrating() || victimSlot < 0 || victimSlot >= Slots)
        {
            return;
        }
        const std::uint32_t fire = _applyingClaim ? _applyingClaimAck : FireFrameOf(attackerSlot);
        if (_applyingClaim)
        {
            launchFrame = _applyingClaimLaunch;
        }
        _lastHitFire[static_cast<std::size_t>(victimSlot)] = launchFrame != 0 ? launchFrame : fire;
        if (attackerSlot >= 0 && attackerSlot < Slots)
        {
            NoteLedger(attackerSlot, victimSlot,
                _applyingClaim ? _applyingClaimAck : fire,
                _applyingClaim ? _applyingClaimLaunch : launchFrame,
                damage, _applyingClaim);
        }
    }

    std::uint32_t NetHitClaims::FireFrameOf(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots || slot == NetSession::LocalSlot()
            || !NetSession::RemoteIntentValid[static_cast<std::size_t>(slot)])
        {
            return NetSession::NetFrame();
        }
        const std::uint32_t ack = NetSession::RemoteIntents[static_cast<std::size_t>(slot)].AckFrame;
        return ack == 0 || ack > NetSession::NetFrame() ? NetSession::NetFrame() : ack;
    }

    void NetHitClaims::Receive(std::int32_t shooterSlot, std::span<const std::uint8_t> payload)
    {
        if (!Arbitrating() || shooterSlot < 0 || shooterSlot >= Slots || payload.empty())
        {
            return;
        }
        const auto s = static_cast<std::size_t>(shooterSlot);
        const std::int32_t count = std::min(static_cast<std::int32_t>(payload[0]), HitClaimPacket::MaxPerPacket);
        for (std::int32_t i = 0; i < count; i++)
        {
            const std::size_t at = static_cast<std::size_t>(1 + i * HitClaimPacket::Size);
            if (at + HitClaimPacket::Size > payload.size())
            {
                break;
            }
            const HitClaimPacket claim = HitClaimPacket::Read(Runtime::SpanSlice(payload, at));
            _received++;
            if (claim.ShooterLifeId == 0 || claim.VictimLifeId == 0
                || !NetSession::MatchesStream(claim.MatchId, claim.AuthorityEpoch)
                || !NetPlayerLifecycle::Matches(shooterSlot, claim.ShooterGeneration, claim.ShooterLifeId))
            {
                NetPlayerLifecycle::OldLifeClaims++;
                continue;
            }
            if (!NetPlayerLifecycle::Matches(claim.VictimSlot, claim.VictimGeneration, claim.VictimLifeId))
            {
                NetPlayerLifecycle::OldLifeClaims++;
                Answer(shooterSlot, claim.ClaimId, HitVerdictPacket::ResultWrongLife, false);
                continue;
            }
            if (Seen(shooterSlot, claim.ClaimId))
            {
                _repeatsHere++;
                const auto seenAt = static_cast<std::size_t>(claim.ClaimId % SeenCapacity);
                const std::uint8_t result = _seenIds[s][seenAt] == claim.ClaimId
                    ? _seenResults[s][seenAt] : HitVerdictPacket::ResultTooOld;
                if (result != ResultPending)
                {
                    Answer(shooterSlot, claim.ClaimId, result);
                }
                continue;
            }
            const auto claimAt = static_cast<std::size_t>(claim.ClaimId % SeenCapacity);
            _seenBeams[s][claimAt] = static_cast<::MphRead::BeamType>(claim.Beam);
            _seenKeys[s][claimAt] = ShotKey::For(shooterSlot, claim.LaunchFrame);
            NetShotDiagnostics::Claims[static_cast<std::size_t>(
                NetShotDiagnostics::Bucket(static_cast<::MphRead::BeamType>(claim.Beam)))]++;
            Remember(shooterSlot, claim.ClaimId, ResultPending);
            const std::uint8_t immediate = Judge(shooterSlot, claim);
            if (immediate != HitVerdictPacket::ResultApplied)
            {
                Answer(shooterSlot, claim.ClaimId, immediate);
                continue;
            }
            Park(shooterSlot, claim);
        }
    }

    bool NetHitClaims::Seen(std::int32_t slot, std::uint16_t id)
    {
        const auto s = static_cast<std::size_t>(slot);
        return id == 0
            || _seenIds[s][static_cast<std::size_t>(id % SeenCapacity)] == id
            || (_newestId[s] != 0 && !NetLifecycleTracker::Newer(id, _newestId[s])
                && static_cast<std::uint16_t>(_newestId[s] - id) >= SeenCapacity);
    }

    void NetHitClaims::Remember(std::int32_t slot, std::uint16_t id, std::uint8_t result)
    {
        const auto s = static_cast<std::size_t>(slot);
        if (_newestId[s] == 0 || NetLifecycleTracker::Newer(id, _newestId[s]))
        {
            _newestId[s] = id;
        }
        const auto at = static_cast<std::size_t>(id % SeenCapacity);
        _seenIds[s][at] = id;
        _seenResults[s][at] = result;
    }

    std::uint8_t NetHitClaims::Judge(std::int32_t shooterSlot, const HitClaimPacket& claim)
    {
        const std::int32_t victimSlot = claim.VictimSlot;
        if (victimSlot < 0 || victimSlot >= Slots || victimSlot == shooterSlot
            || victimSlot >= static_cast<std::int32_t>(PlayerEntity::Players().size()))
        {
            _refusedHere++;
            return HitVerdictPacket::ResultRefused;
        }
        const std::uint32_t now = NetSession::NetFrame();
        if (claim.AckFrame == 0 || claim.AckFrame > now
            || now - claim.AckFrame > static_cast<std::uint32_t>(MaxClaimAge()))
        {
            _tooOldHere++;
            return HitVerdictPacket::ResultTooOld;
        }
        if (claim.LaunchFrame > claim.AckFrame)
        {
            _refusedHere++;
            return HitVerdictPacket::ResultInvalidLaunch;
        }
        if (claim.Damage > MaxDamageFor(claim.Beam))
        {
            _refusedHere++;
            NetLog::Event("slot " + std::to_string(shooterSlot) + " claimed " + std::to_string(claim.Damage)
                + " damage with beam " + std::to_string(claim.Beam) + ", which cannot deal more than "
                + std::to_string(MaxDamageFor(claim.Beam)));
            return HitVerdictPacket::ResultDamageLimit;
        }
        OpenTK::Mathematics::Vector3 was{};
        if (!NetUnlagged::PositionAt(victimSlot, claim.AckFrame, claim.VictimGeneration, claim.VictimLifeId, was))
        {
            _tooOldHere++;
            return HitVerdictPacket::ResultTooOld;
        }
        const float reach = claim.Beam == HitClaimPacket::NoBeam ? MeleeRadius : ClaimRadius;
        const OpenTK::Mathematics::Vector3 offset = claim.HitPoint - was;
        if (!std::isfinite(offset.X) || !std::isfinite(offset.Y) || !std::isfinite(offset.Z)
            || offset.LengthSquared() > reach * reach)
        {
            _refusedHere++;
            NetLog::Event("slot " + std::to_string(shooterSlot) + " claimed a hit on slot " + std::to_string(victimSlot)
                + " at " + claim.HitPoint.ToString() + ", "
                + Runtime::ToString(OpenTK::Mathematics::Length(offset), "F2") + " units from where frame "
                + std::to_string(claim.AckFrame) + " put them");
            return HitVerdictPacket::ResultGeometry;
        }
        PlayerEntity& victim = PlayerAt(victimSlot);
        if (!HasLoadFlag(victim.LoadFlags(), LoadFlags::Active) || !victim.ModIsInPlay())
        {
            _voidedDeadVictim++;
            return HitVerdictPacket::ResultDeadVictim;
        }
        const std::uint32_t fired = claim.LaunchFrame != 0 ? claim.LaunchFrame : claim.AckFrame;
        const auto s = static_cast<std::size_t>(shooterSlot);
        if (_dead[s] && _deathFire[s] < fired)
        {
            _voidedDeadShooter++;
            return HitVerdictPacket::ResultDeadShooter;
        }
        return HitVerdictPacket::ResultApplied;
    }

    void NetHitClaims::NoteAgreement(std::int32_t shooter, std::int32_t victim, std::uint8_t beam,
        std::int32_t claimed, std::int32_t resolved)
    {
        if (resolved <= 0)
        {
            return;
        }
        const std::int32_t bucket = beam == HitClaimPacket::NoBeam || beam >= NetHitPrediction::AltBeam
            ? NetHitPrediction::AltBeam
            : beam;
        const auto b = static_cast<std::size_t>(bucket);
        _claimedByBeam[b] += claimed;
        _resolvedByBeam[b] += resolved;
        if (claimed == resolved)
        {
            _agreeByBeam[b]++;
            return;
        }
        _differByBeam[b]++;
        if (_disagreementsLogged < 20)
        {
            _disagreementsLogged++;
            NetLog::Event("slot " + std::to_string(shooter) + " predicted " + std::to_string(claimed)
                + " damage on slot " + std::to_string(victim) + " with beam " + std::to_string(beam)
                + " and this machine resolved " + std::to_string(resolved));
        }
    }

    std::string NetHitClaims::DescribeAgreement()
    {
        std::string text = "predicted vs resolved damage:";
        bool any = false;
        for (std::size_t i = 0; i < _agreeByBeam.size(); i++)
        {
            const std::int64_t paired = _agreeByBeam[i] + _differByBeam[i];
            if (paired == 0)
            {
                continue;
            }
            any = true;
            const std::string name = static_cast<std::int32_t>(i) == NetHitPrediction::AltBeam
                ? std::string("alt/bomb")
                : ::MphRead::ToString(static_cast<::MphRead::BeamType>(i));
            text += "\n  " + Runtime::StringPadRight(name, 13) + " "
                + Runtime::StringPadLeft(std::to_string(paired), 5) + " paired, "
                + Runtime::StringPadLeft(std::to_string(_agreeByBeam[i]), 5) + " agreed ("
                + Runtime::ToString(static_cast<double>(_agreeByBeam[i]) * 100.0 / static_cast<double>(paired), "F0")
                + "%), " + Runtime::StringPadLeft(std::to_string(_differByBeam[i]), 5) + " differed -- "
                + std::to_string(_claimedByBeam[i]) + " claimed against " + std::to_string(_resolvedByBeam[i])
                + " resolved";
        }
        return any ? text : std::string("predicted vs resolved damage: nothing paired");
    }

    void NetHitClaims::Park(std::int32_t shooterSlot, const HitClaimPacket& claim)
    {
        std::int32_t index = -1;
        for (std::int32_t i = 0; i < PendingCapacity; i++)
        {
            if (!_pending[static_cast<std::size_t>(i)].Live)
            {
                index = i;
                break;
            }
        }
        if (index < 0)
        {
            return;
        }
        Pending entry;
        entry.MatchId = claim.MatchId;
        entry.AuthorityEpoch = claim.AuthorityEpoch;
        entry.ShooterGeneration = claim.ShooterGeneration;
        entry.ShooterLifeId = claim.ShooterLifeId;
        entry.VictimGeneration = claim.VictimGeneration;
        entry.VictimLifeId = claim.VictimLifeId;
        entry.Id = claim.ClaimId;
        entry.ShooterSlot = static_cast<std::uint8_t>(shooterSlot);
        entry.VictimSlot = claim.VictimSlot;
        entry.Beam = claim.Beam;
        entry.Damage = claim.Damage;
        entry.Flags = claim.Flags;
        entry.AckFrame = claim.AckFrame;
        entry.LaunchFrame = claim.LaunchFrame;
        entry.HitPoint = claim.HitPoint;
        entry.Arrived = NetSession::NetFrame();
        entry.Grace = GraceFor(shooterSlot);
        entry.Live = true;
        _pending[static_cast<std::size_t>(index)] = entry;
    }

    std::int32_t NetHitClaims::MaxDamageFor(std::uint8_t beam)
    {
        std::int32_t raw = 200;
        const auto& current = ::MphRead::Weapons::Current;
        if (beam != HitClaimPacket::NoBeam && current != nullptr && beam < current->size())
        {
            const ::MphRead::WeaponInfo& info = Runtime::RequireReference((*current)[beam]);
            raw = std::max<std::int32_t>(info.ChargedHeadshotDamage,
                std::max<std::int32_t>(info.HeadshotDamage,
                std::max<std::int32_t>(info.MinChargeHeadshotDamage,
                std::max<std::int32_t>(info.ChargedDamage,
                std::max<std::int32_t>(info.UnchargedDamage,
                std::max<std::int32_t>(info.MinChargeDamage, info.ChargedSplashDamage))))));
        }
        return static_cast<std::int32_t>(static_cast<float>(raw) * 5.0F) + 1;
    }

    void NetHitClaims::Tick()
    {
        if (!NetRoomChange::GameplayReady())
        {
            return;
        }
        if (Claiming())
        {
            TickOutbox();
        }
        if (!Arbitrating())
        {
            return;
        }
        TrackDeaths();
        const std::uint32_t now = NetSession::NetFrame();
        while (true)
        {
            std::int32_t next = -1;
            std::int32_t overdue = -1;
            for (std::int32_t i = 0; i < PendingCapacity; i++)
            {
                Pending& entry = _pending[static_cast<std::size_t>(i)];
                if (!entry.Live)
                {
                    continue;
                }
                if (!NetSession::MatchesStream(entry.MatchId, entry.AuthorityEpoch)
                    || !NetPlayerLifecycle::Matches(entry.ShooterSlot, entry.ShooterGeneration, entry.ShooterLifeId)
                    || !NetPlayerLifecycle::Matches(entry.VictimSlot, entry.VictimGeneration, entry.VictimLifeId))
                {
                    entry.Live = false;
                    NetPlayerLifecycle::OldLifeClaims++;
                    continue;
                }
                std::int32_t resolved = 0;
                if (TakeLedger(entry.ShooterSlot, entry.VictimSlot, entry.AckFrame,
                        entry.LaunchFrame, entry.Arrived, entry.Grace, resolved))
                {
                    entry.Live = false;
                    _duplicateHere++;
                    NoteAgreement(entry.ShooterSlot, entry.VictimSlot, entry.Beam, entry.Damage, resolved);
                    Answer(entry.ShooterSlot, entry.Id, HitVerdictPacket::ResultDuplicate);
                    continue;
                }
                if (next < 0)
                {
                    next = i;
                }
                else
                {
                    const Pending& best = _pending[static_cast<std::size_t>(next)];
                    if ((entry.LaunchFrame != 0 ? entry.LaunchFrame : entry.AckFrame)
                        < (best.LaunchFrame != 0 ? best.LaunchFrame : best.AckFrame))
                    {
                        next = i;
                    }
                }
                if (now - entry.Arrived >= 2 * MaxGraceFrames
                    && (overdue < 0 || entry.Arrived < _pending[static_cast<std::size_t>(overdue)].Arrived))
                {
                    overdue = i;
                }
            }
            if (next < 0)
            {
                break;
            }
            if (now - _pending[static_cast<std::size_t>(next)].Arrived
                < static_cast<std::uint32_t>(_pending[static_cast<std::size_t>(next)].Grace))
            {
                if (overdue < 0)
                {
                    break;
                }
                next = overdue;
            }
            ApplyOne(_pending[static_cast<std::size_t>(next)]);
            _pending[static_cast<std::size_t>(next)].Live = false;
            TrackDeaths();
        }
        FlushVerdicts();
    }

    void NetHitClaims::TrackDeaths()
    {
        for (std::int32_t i = 0; i < Slots; i++)
        {
            const auto s = static_cast<std::size_t>(i);
            const bool inPlay = i < static_cast<std::int32_t>(PlayerEntity::Players().size())
                && HasLoadFlag(PlayerAt(i).LoadFlags(), LoadFlags::Active)
                && PlayerAt(i).ModIsInPlay();
            if (_wasInPlay[s] && !inPlay)
            {
                _dead[s] = true;
                _deathFire[s] = _lastHitFire[s] != 0 ? _lastHitFire[s] : NetSession::NetFrame();
            }
            else if (!_wasInPlay[s] && inPlay)
            {
                _dead[s] = false;
                _deathFire[s] = 0;
                _lastHitFire[s] = 0;
                for (std::int32_t j = 0; j < Slots; j++)
                {
                    ClearLedger(j, i);
                }
            }
            _wasInPlay[s] = inPlay;
        }
    }

    void NetHitClaims::NoteRescued(std::int32_t attacker, std::int32_t victim, std::uint32_t launch)
    {
        if (launch == 0)
        {
            return;
        }
        for (std::size_t i = 0; i < RescuedCapacity; i++)
        {
            if (_rescuedOwed[i] > 0 && _rescuedKeys[i] == ShotKey::For(attacker, launch)
                && _rescuedVictim[i] == victim
                && NetPlayerLifecycle::Matches(victim, _rescuedVictimGeneration[i], _rescuedVictimLife[i]))
            {
                _rescuedOwed[i]++;
                _rescuedAt[i] = NetSession::NetFrame();
                return;
            }
        }
        const auto at = static_cast<std::size_t>(_rescuedHead);
        _rescuedHead = (_rescuedHead + 1) % RescuedCapacity;
        _rescuedAttacker[at] = static_cast<std::uint8_t>(attacker);
        _rescuedVictim[at] = static_cast<std::uint8_t>(victim);
        _rescuedKeys[at] = ShotKey::For(attacker, launch);
        _rescuedVictimGeneration[at] = NetPlayerLifecycle::Generation(victim);
        _rescuedVictimLife[at] = NetPlayerLifecycle::Get(victim);
        _rescuedAt[at] = NetSession::NetFrame();
        _rescuedOwed[at] = 1;
    }

    bool NetHitClaims::AlreadyRescued(std::int32_t attacker, std::int32_t victim, std::uint32_t launch,
        std::optional<ShotKey> launchKey)
    {
        if (!Arbitrating() || launch == 0 || _applyingClaim
            || attacker < 0 || attacker >= Slots || victim < 0 || victim >= Slots)
        {
            return false;
        }
        const std::uint32_t now = NetSession::NetFrame();
        for (std::size_t i = 0; i < RescuedCapacity; i++)
        {
            if (_rescuedOwed[i] <= 0 || _rescuedKeys[i] != launchKey.value_or(ShotKey::For(attacker, launch))
                || _rescuedVictim[i] != victim
                || !NetPlayerLifecycle::Matches(victim, _rescuedVictimGeneration[i], _rescuedVictimLife[i]))
            {
                continue;
            }
            if (now - _rescuedAt[i] > RescuedFrames)
            {
                _rescuedOwed[i] = 0;
                continue;
            }
            _rescuedOwed[i]--;
            _suppressedHere++;
            NetLog::Event("refused the authority's own copy of slot " + std::to_string(attacker)
                + "'s shot (launch " + std::to_string(launch) + ") on slot " + std::to_string(victim)
                + ": a claim already made it real");
            return true;
        }
        return false;
    }

    void NetHitClaims::ApplyOne(Pending& entry)
    {
        const std::int32_t victimSlot = entry.VictimSlot;
        const std::int32_t shooterSlot = entry.ShooterSlot;
        if (!NetSession::MatchesStream(entry.MatchId, entry.AuthorityEpoch)
            || !NetPlayerLifecycle::Matches(shooterSlot, entry.ShooterGeneration, entry.ShooterLifeId)
            || !NetPlayerLifecycle::Matches(victimSlot, entry.VictimGeneration, entry.VictimLifeId))
        {
            NetPlayerLifecycle::OldLifeClaims++;
            return;
        }
        if (victimSlot >= static_cast<std::int32_t>(PlayerEntity::Players().size())
            || shooterSlot >= static_cast<std::int32_t>(PlayerEntity::Players().size()))
        {
            Answer(shooterSlot, entry.Id, HitVerdictPacket::ResultRefused);
            return;
        }
        PlayerEntity& victim = PlayerAt(victimSlot);
        PlayerEntity& shooter = PlayerAt(shooterSlot);
        if (!HasLoadFlag(victim.LoadFlags(), LoadFlags::Active) || !victim.ModIsInPlay())
        {
            _voidedDeadVictim++;
            Answer(shooterSlot, entry.Id, HitVerdictPacket::ResultDeadVictim);
            return;
        }
        const auto s = static_cast<std::size_t>(shooterSlot);
        if (_dead[s] && _deathFire[s] < (entry.LaunchFrame != 0 ? entry.LaunchFrame : entry.AckFrame))
        {
            _voidedDeadShooter++;
            Answer(shooterSlot, entry.Id, HitVerdictPacket::ResultDeadShooter);
            return;
        }
        DamageFlags flags = DamageFlags::NoDmgInvuln;
        if ((entry.Flags & HitClaimPacket::FlagHeadshot) != 0)
        {
            flags = static_cast<DamageFlags>(static_cast<std::int32_t>(flags)
                | static_cast<std::int32_t>(DamageFlags::Headshot));
        }
        _applyingClaim = true;
        _applyingClaimAck = entry.AckFrame;
        _applyingClaimLaunch = entry.LaunchFrame;
        const bool lethal = victim.Health() <= entry.Damage;
        const auto before = static_cast<std::uint32_t>(victim.Health());
        try
        {
            const NetDamage::ClaimScope scope(entry.Beam == HitClaimPacket::NoBeam
                ? ::MphRead::BeamType::None : static_cast<::MphRead::BeamType>(entry.Beam));
            victim.TakeDamage(static_cast<std::uint32_t>(entry.Damage), flags, std::nullopt, &shooter);
        }
        catch (...)
        {
            _applyingClaim = false;
            throw;
        }
        _applyingClaim = false;
        if (static_cast<std::uint32_t>(victim.Health()) >= before)
        {
            _refusedHere++;
            Answer(shooterSlot, entry.Id, HitVerdictPacket::ResultNoDamage);
            return;
        }
        if ((entry.Flags & HitClaimPacket::FlagFrozen) != 0 && victim.Health() > 0)
        {
            victim.ModSetFrozen(true);
        }
        _appliedHere++;
        NoteRescued(shooterSlot, victimSlot, entry.LaunchFrame);
        const std::uint32_t dealt = before - static_cast<std::uint32_t>(std::max(0, victim.Health()));
        _rescuedDamage += dealt;
        if ((entry.Flags & HitClaimPacket::FlagHeadshot) != 0)
        {
            _rescuedHeadshots++;
        }
        if (lethal)
        {
            _rescuedKills++;
        }
        NetLog::Event("rescue " + std::to_string(entry.Id) + " beam="
            + (entry.Beam == HitClaimPacket::NoBeam ? std::string("none")
                : ::MphRead::ToString(static_cast<::MphRead::BeamType>(entry.Beam)))
            + " nearest=" + std::to_string(NearestLedgerOffset(shooterSlot, victimSlot, entry.Arrived)) + "f"
            + " window=" + std::to_string(entry.Grace) + "f ack=" + std::to_string(entry.AckFrame)
            + " arrived=" + std::to_string(entry.Arrived));
        NetLog::Event("claim " + std::to_string(entry.Id) + ": slot " + std::to_string(shooterSlot) + " hit slot "
            + std::to_string(victimSlot) + " for " + std::to_string(dealt)
            + ((entry.Flags & HitClaimPacket::FlagHeadshot) != 0 ? " (headshot)" : "")
            + (lethal ? " and killed them" : "")
            + ", aimed at frame " + std::to_string(entry.AckFrame) + ", "
            + std::to_string(NetSession::NetFrame() - entry.AckFrame) + " frames ago"
            + (_dead[s] ? " -- and was dead by the time it arrived" : ""));
        Answer(shooterSlot, entry.Id, HitVerdictPacket::ResultApplied);
    }

    void NetHitClaims::Answer(std::int32_t slot, std::uint16_t id, std::uint8_t result, bool remember)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        const auto s = static_cast<std::size_t>(slot);
        const auto at = static_cast<std::size_t>(id % SeenCapacity);
        if (remember && _seenIds[s][at] == id && _seenResults[s][at] == ResultPending)
        {
            const auto weapon = static_cast<std::size_t>(NetShotDiagnostics::Bucket(_seenBeams[s][at]));
            if (result == HitVerdictPacket::ResultApplied)
            {
                NetShotDiagnostics::Rescues[weapon]++;
            }
            else if (result != HitVerdictPacket::ResultDuplicate)
            {
                NetShotDiagnostics::Refusals[weapon]++;
            }
            if (NetLog::Enabled())
            {
                NetShotDiagnostics::Trace("authority-verdict", _seenKeys[s][at], _seenBeams[s][at],
                    "claim=" + std::to_string(id) + " result=" + HitVerdictPacket::Describe(result));
            }
        }
        if (remember)
        {
            Remember(slot, id, result);
        }
        if (_verdictCount[s] >= VerdictCapacity)
        {
            return;
        }
        _verdicts[s][static_cast<std::size_t>(_verdictCount[s]++)] = {id, result};
    }

    void NetHitClaims::FlushVerdicts()
    {
        if (!_verdictSink)
        {
            _verdictCount.fill(0);
            return;
        }
        std::array<std::pair<std::uint16_t, std::uint8_t>, VerdictCapacity> scratch{};
        for (std::int32_t slot = 0; slot < Slots; slot++)
        {
            const auto s = static_cast<std::size_t>(slot);
            const std::int32_t count = _verdictCount[s];
            if (count == 0)
            {
                continue;
            }
            for (std::size_t i = 0; i < static_cast<std::size_t>(count); i++)
            {
                scratch[i] = _verdicts[s][i];
            }
            _verdictCount[s] = 0;
            _verdictSink(slot, std::span<const std::pair<std::uint16_t, std::uint8_t>>(
                scratch.data(), static_cast<std::size_t>(count)));
        }
    }

    void NetHitClaims::Reset()
    {
        _outbox.fill(Outgoing{});
        _pending.fill(Pending{});
        for (auto& row : _seenIds) row.fill(0);
        for (auto& row : _seenResults) row.fill(0);
        _newestId.fill(0);
        _lastResult.fill(0);
        _authorityHit = {};
        _authorityHitAck = {};
        _authorityHitLaunch = {};
        _authorityHitUsed = {};
        for (auto& row : _authorityHitHead) row.fill(0);
        _deathFire.fill(0);
        _dead.fill(false);
        _lastHitFire.fill(0);
        _wasInPlay.fill(false);
        _verdictCount.fill(0);
        _rescuedOwed.fill(0);
        _rescuedHead = 0;
        _nextId = 1;
        _declared = 0;
        _applied = 0;
        _duplicate = 0;
        _refusedDeadShooter = 0;
        _refusedDeadVictim = 0;
        _refusedOther = 0;
        _unanswered = 0;
        _resends = 0;
        _received = 0;
        _appliedHere = 0;
        _duplicateHere = 0;
        _voidedDeadShooter = 0;
        _voidedDeadVictim = 0;
        _refusedHere = 0;
        _tooOldHere = 0;
        _repeatsHere = 0;
        _suppressedHere = 0;
        _matchedByLaunch = 0;
        _matchedByWindow = 0;
        _rescuedDamage = 0;
        _agreeByBeam.fill(0);
        _differByBeam.fill(0);
        _claimedByBeam.fill(0);
        _resolvedByBeam.fill(0);
        _disagreementsLogged = 0;
        _rescuedKills = 0;
        _rescuedHeadshots = 0;
    }

    void NetHitClaims::ForgetSlot(std::int32_t slot, bool preserveFlights)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        const auto s = static_cast<std::size_t>(slot);
        for (Outgoing& entry : _outbox)
        {
            if (entry.VictimSlot == slot || slot == NetSession::LocalSlot())
            {
                entry.Live = false;
            }
        }
        for (Pending& entry : _pending)
        {
            if (entry.VictimSlot == slot || entry.ShooterSlot == slot)
            {
                entry.Live = false;
            }
        }
        _verdictCount[s] = 0;
        for (std::size_t i = 0; i < SeenCapacity; i++)
        {
            _seenIds[s][i] = 0;
            _seenResults[s][i] = 0;
        }
        for (std::size_t i = 0; i < RescuedCapacity; i++)
        {
            if ((!preserveFlights && _rescuedAttacker[i] == slot) || _rescuedVictim[i] == slot)
            {
                _rescuedOwed[i] = 0;
            }
        }
        _newestId[s] = 0;
        _lastResult[s] = 0;
        _deathFire[s] = 0;
        _dead[s] = false;
        _lastHitFire[s] = 0;
        _wasInPlay[s] = false;
        for (std::int32_t i = 0; i < Slots; i++)
        {
            ClearLedger(slot, i);
            ClearLedger(i, slot);
        }
    }

    void NetHitClaims::ForgetPending()
    {
        for (auto& row : _seenIds) row.fill(0);
        for (auto& row : _seenResults) row.fill(0);
        _newestId.fill(0);
        _verdictCount.fill(0);
        _rescuedOwed.fill(0);
        _outbox.fill(Outgoing{});
        _pending.fill(Pending{});
        _authorityHit = {};
        _authorityHitAck = {};
        _authorityHitUsed = {};
        for (auto& row : _authorityHitHead) row.fill(0);
        _deathFire.fill(0);
        _dead.fill(false);
        _lastHitFire.fill(0);
        _wasInPlay.fill(false);
    }

    std::optional<std::string> NetHitClaims::Describe()
    {
        if (_declared == 0 && _received == 0)
        {
            return std::nullopt;
        }
        if (_received > 0)
        {
            return "hit claims (as authority): " + std::to_string(_received) + " received, "
                + std::to_string(_appliedHere) + " applied (" + std::to_string(_rescuedDamage) + " damage, "
                + std::to_string(_rescuedKills) + " kills, " + std::to_string(_rescuedHeadshots)
                + " headshots rescued), " + std::to_string(_duplicateHere) + " already resolved, "
                + std::to_string(_voidedDeadShooter) + " from a shooter already dead, "
                + std::to_string(_voidedDeadVictim) + " on a victim already down, "
                + std::to_string(_refusedHere) + " refused, " + std::to_string(_tooOldHere) + " too old, "
                + std::to_string(_repeatsHere) + " repeats "
                + "(matched " + std::to_string(_matchedByLaunch) + " by shot, " + std::to_string(_matchedByWindow)
                + " by window, " + std::to_string(_suppressedHere) + " of its own refused as already rescued)";
        }
        const std::int64_t answered = _applied + _duplicate + _refusedDeadShooter + _refusedDeadVictim + _refusedOther;
        const double pct = answered > 0
            ? 100.0 * static_cast<double>(_applied + _duplicate) / static_cast<double>(answered) : 0.0;
        return "hit claims: " + std::to_string(_declared) + " declared, " + std::to_string(_applied) + " applied, "
            + std::to_string(_duplicate) + " already resolved (" + Runtime::ToString(pct, "F1") + "% stood), "
            + std::to_string(_refusedDeadShooter) + " void (dead shooter), "
            + std::to_string(_refusedDeadVictim) + " void (victim down), " + std::to_string(_refusedOther)
            + " refused, " + std::to_string(_unanswered) + " unanswered, " + std::to_string(_resends) + " repeats";
    }
}
