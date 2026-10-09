#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
#include "FormReconciliation.hpp"
#include "NetProtocol.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    class Keybind;
    class PlayerControls;
    class BeamProjectileEntity;
}

namespace MphRead::Mods::Network
{
    // NetPlayerBridge's confirmed impacts: a damage that arrived before its
    // shot appeared here, and the shots that already went by.
    struct ConfirmPending
    {
        std::int32_t Attacker = -1;
        std::uint8_t LaunchLow = 0;
        std::uint8_t Beam = 0;
        OpenTK::Mathematics::Vector3 Offset{};
        std::uint32_t Until = 0;
        bool Live = false;
        bool Headshot = false;
        std::int32_t Victim = -1;
    };
    struct ConfirmGoneShot
    {
        std::int32_t Attacker = -1;
        std::uint8_t LaunchLow = 0;
        std::uint32_t Frame = 0;
        // A shot that went out in a blast: where, and how far it reached.
        OpenTK::Mathematics::Vector3 Where{};
        float Blast = 0;
    };

    class NetPlayerBridge final
    {
    public:
        NetPlayerBridge() = delete;

        inline static std::int32_t PlacementsRefused = 0;
        inline static std::int32_t SpawnFacingsTurned = 0;
        inline static float WorstSpawnFacing = 0.0F;
        inline static std::int32_t StaleDeathsIgnored = 0;
        inline static std::int64_t NodeLookupsUnresolved = 0;

        [[nodiscard]] static std::string FormSaidByAuthority();

        [[nodiscard]] static std::int64_t RejectedUpdates() noexcept { return _rejectedUpdates; }
        [[nodiscard]] static std::int64_t Snaps() noexcept { return _snaps; }
        [[nodiscard]] static float WorstSnap() noexcept { return _worstSnap; }

        [[nodiscard]] static OpenTK::Mathematics::Vector3 InFormFor(
            Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position, bool measuredInAlt);

        static void RecordPresses(Entities::PlayerEntity& player);
        [[nodiscard]] static IntentPacket CaptureIntent(Entities::PlayerEntity& player);

        [[nodiscard]] static bool RespawnRequested(std::int32_t slot);
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity> ShootPressAge{};
        static void ApplyIntent(Entities::PlayerEntity& player, const IntentPacket& intent);
        static void NoteSpawn(std::int32_t slot);
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity> SpawnFrame{};
        [[nodiscard]] static bool AimTrusted(std::int32_t slot);
        [[nodiscard]] static bool AimAvailable(std::int32_t slot);

        static void ApplyState(Entities::PlayerEntity& player, const PlayerState& state, bool isLocal);
        [[nodiscard]] static FormCorrection ReconcileForm(std::int32_t slot, std::uint32_t frame, bool desiredAlt,
            bool actualAlt, bool morphing, bool unmorphing, std::int32_t ping);

        static void NoteRoomChanged();
        static void Reset();
        static void ForgetSlot(std::int32_t slot);
        // The ray this machine's own player fired on this frame, after spread.
        static void NoteLocalShot(OpenTK::Mathematics::Vector3 origin, OpenTK::Mathematics::Vector3 direction) noexcept;
        static void AttachLocalShot(IntentPacket& intent) noexcept;
        // The victim's view of a remote shot. A remote player's aim is the
        // one they had on their own screen, where this player stood a round
        // trip ago; drawn as is, the shot sails past and the damage lands
        // anyway. When that aim was on this player in the world the shooter
        // was looking at, it is turned onto where this player is now, so the
        // shot that hits is also the shot that is seen hitting.
        [[nodiscard]] static OpenTK::Mathematics::Vector3 RetargetAtLocal(const Entities::PlayerEntity& shooter,
            OpenTK::Mathematics::Vector3 origin, OpenTK::Mathematics::Vector3 aim, std::uint32_t ackFrame,
            OpenTK::Mathematics::Vector3 aimedFrom);
        // The shooter's own ray, when the intent carries it, else its aim
        // from where it reported standing (plus the drawn gun's offset).
        static void ShooterRay(const Entities::PlayerEntity& shooter, OpenTK::Mathematics::Vector3 drawnMuzzle,
            OpenTK::Mathematics::Vector3& from, OpenTK::Mathematics::Vector3& direction, std::uint32_t& ackFrame);
        [[nodiscard]] static std::int64_t AimsRetargeted() noexcept { return _aimsRetargeted; }
        // The authority says that player hit this one: if one of their shots
        // drawn here is still in the air, it is the one, and it is turned onto
        // this player so the hit is seen arriving with the damage.
        static void SteerIncoming(std::int32_t attackerSlot);

        // Confirmed impacts (protocol 18). The authority's damage names the
        // shot (the low byte of its launch frame) and where on this player the
        // shooter saw it land. That one shot is the one drawn arriving there:
        // homed onto the spot if it is in the air here, homed from the moment
        // it appears if it has not appeared yet, and drawn as an impact on the
        // spot if it is already gone. A remote shot nobody confirmed passes
        // through this player instead of being drawn hitting them -- the
        // impacts the shooter never made.
        static void SetScene(MphRead::Scene* scene) noexcept { _scene = scene; }
        static void ConfirmedImpacts(bool value) noexcept { _confirmedImpacts = value; }
        [[nodiscard]] static bool ConfirmedImpacts() noexcept { return _confirmedImpacts; }
        // keyed: the damage event is the player state's newest, the one the
        // launch byte and impact belong to; an older one is drawn at the chest.
        static void ConfirmIncoming(std::int32_t attackerSlot, std::int32_t victimSlot, std::uint8_t beam,
            bool keyed, std::uint8_t launchLow, ImpactOffset impact, bool headshot = false);
        // Confirmed impacts drawn between two other players too (this
        // machine watching A hit B); -noobservedimpacts keeps them to this player.
        static void ObservedImpacts(bool value) noexcept { _observedImpacts = value; }
        // The Shock Coil: while its ticks keep being confirmed on a player,
        // the beam drawn from that shooter is pointed at the spot the ticks
        // land on. True (and aim set) when it is.
        static bool CoilAimFor(const Entities::PlayerEntity& shooter, OpenTK::Mathematics::Vector3 muzzle,
            OpenTK::Mathematics::Vector3& aim);
        static void OnRemoteShotSpawned(Entities::BeamProjectileEntity& beam);
        static void NoteRemoteShotGone(const Entities::BeamProjectileEntity& beam);
        // Called for every player a projectile meets: true when this is a
        // remote shot nobody has confirmed against that player.
        [[nodiscard]] static bool PassesThrough(Entities::BeamProjectileEntity& beam,
            const Entities::PlayerEntity& player);
        static void TickConfirms();
        // How long a remote shot that met a player waits for the word: the
        // 90th percentile of how long the word actually took on this line
        // (the last 64 held shots that were confirmed), plus one, within 2-8;
        // 6 until 16 have been seen. A miss is held no longer than it must be.
        [[nodiscard]] static std::uint32_t HoldFrames() noexcept;
        static void NoteConfirmDelay(std::uint32_t frames) noexcept;
        static void NotePassedLocal(const Entities::BeamProjectileEntity& beam);
        static void SynthesizeImpact(std::int32_t attackerSlot, std::int32_t victimSlot, std::uint8_t beam,
            std::uint8_t launchLow, OpenTK::Mathematics::Vector3 offset, bool headshot = false);
        [[nodiscard]] static std::string DescribeConfirms();
        [[nodiscard]] static std::int64_t ShotsSteered() noexcept { return _shotsSteered; }
        static void RetargetEnabled(bool value) noexcept { _retargetEnabled = value; }
        [[nodiscard]] static std::int64_t KillsResynced() noexcept { return _killsResynced; }
        // Apply the next snapshot of this slot as a fresh life: used when a
        // kill shown here is refused, so the player is put back where and as
        // the authority has them.
        static void Resync(std::int32_t slot) noexcept
        {
            if (slot >= 0 && slot < Slots) _lifeApplied[static_cast<std::size_t>(slot)] = false;
        }

        static void ApplyReportedPosition(Entities::PlayerEntity& player, const IntentPacket& intent);
        static void RestoreSnapshotPosition(Entities::PlayerEntity& player, const PlayerState& state);
        static void RestoreReportedPosition(Entities::PlayerEntity& player, const IntentPacket& intent);

    private:
        static constexpr std::int32_t Slots = Entities::PlayerEntity::SlotCapacity;
        static constexpr float SnapDistance = 15.0F;
        [[maybe_unused]] static constexpr float CatchUpRate = 0.35F;
        [[maybe_unused]] static constexpr float FastCatchUpRate = 0.6F;
        [[maybe_unused]] static constexpr float FastCatchUpAbove = 3.0F;
        static constexpr float DesyncDistance = 30.0F;
        static constexpr float MaxReportedSpeed = 5.0F;
        static constexpr float PositionLimit = 100000.0F;
        static constexpr std::uint32_t AimHoldCeiling = 90;
        static constexpr std::int32_t DivergedFramesBeforeCorrecting = 60;
        static constexpr IntentButtons PressedButtons = ~(
            IntentButtons::ZoomedState | IntentButtons::AltFormState
            | IntentButtons::InPlayState | IntentButtons::SpectatingState
            | IntentButtons::ReadyState);

        [[nodiscard]] static OpenTK::Mathematics::Vector3 InForm(
            Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position, bool measuredInAlt);
        [[nodiscard]] static bool Sane(OpenTK::Mathematics::Vector3 value) noexcept;
        [[nodiscard]] static IntentButtons MissedPresses(std::int32_t slot, const IntentPacket& intent,
            std::int32_t& shootAge);
        static void Set(Entities::Keybind& bind, bool down, bool pressed = false);
        static void BeginRemoteLife(Entities::PlayerEntity& player, const PlayerState& state);
        static void ApplyAfflictions(Entities::PlayerEntity& player, PlayerState state);
        static void ApplyForm(Entities::PlayerEntity& player, bool altForm);
        [[nodiscard]] static bool Diverged(Entities::PlayerEntity& player, const PlayerState& state, std::int32_t slot);
        [[nodiscard]] static bool StaleSinceSpawn(Entities::PlayerEntity& player, const IntentPacket& intent);
        static void NoteReportedVelocity(Entities::PlayerEntity& player,
            OpenTK::Mathematics::Vector3 reported, std::uint32_t frame);
        [[nodiscard]] static bool FrozenInPlace(Entities::PlayerEntity& player);
        static void Move(Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position);

        inline static std::array<FormReconciliation, Slots> _formReconciliation{};
        inline static std::array<std::uint8_t, Slots> _formSaid{};

        inline static std::int64_t _rejectedUpdates = 0;
        inline static std::int64_t _snaps = 0;
        inline static float _worstSnap = 0.0F;

        inline static std::array<std::uint32_t, IntentPacket::PressHistory> _pressHistory{};
        inline static std::int32_t _latchedCharge = 0;
        inline static std::int32_t _latchedBoostDamage = 0;
        inline static bool _hasLatch = false;

        inline static std::array<std::uint32_t, Slots> _lastPressFrame{};
        inline static std::array<bool, Slots> _pressSeen{};
        inline static std::array<bool, Slots> _respawnRequested{};
        inline static std::array<bool, Slots> _aimHeld{};

        inline static std::array<std::uint16_t, Slots> _appliedLifeId{};
        inline static std::array<bool, Slots> _lifeApplied{};
        inline static std::int64_t _killsResynced = 0;
        static constexpr std::int32_t LocalHistory = 128;
        inline static std::array<std::uint32_t, LocalHistory> _localFrames{};
        inline static std::array<OpenTK::Mathematics::Vector3, LocalHistory> _localPositions{};
        inline static std::int64_t _aimsRetargeted = 0;
        inline static std::int64_t _shotsSteered = 0;
        static constexpr std::uint32_t ConfirmWaitFrames = 10;
        inline static MphRead::Scene* _scene = nullptr;
        inline static bool _confirmedImpacts = true;
        inline static std::array<ConfirmPending, 16> _pendingConfirms{};
        inline static std::array<ConfirmGoneShot, 32> _goneShots{};
        inline static std::size_t _goneNext = 0;
        inline static std::int64_t _confirmsInFlight = 0;
        inline static std::int64_t _confirmsAtSpawn = 0;
        inline static std::int64_t _impactsSynthesized = 0;
        inline static std::int64_t _passedThrough = 0;
        inline static std::int64_t _confirmsIgnored = 0;
        inline static double _homeAngleSum = 0;
        inline static bool _observedImpacts = true;
        inline static std::array<std::int32_t, 8> _coilVictim{-1, -1, -1, -1, -1, -1, -1, -1};
        inline static std::array<OpenTK::Mathematics::Vector3, 8> _coilOffset{};
        inline static std::array<std::uint32_t, 8> _coilUntil{};
        inline static std::int64_t _coilTicks = 0;
        inline static std::int64_t _unkeyedImpacts = 0;
        inline static std::int64_t _observedConfirms = 0;
        inline static double _homeAngleMax = 0;
        [[nodiscard]] static const ConfirmGoneShot* RecentlyGone(std::int32_t attackerSlot, std::uint8_t launchLow);
        inline static std::int64_t _seenAsBlast = 0;
        inline static std::array<std::uint8_t, 64> _confirmDelays{};
        inline static std::size_t _confirmDelayCount = 0;
        inline static std::size_t _confirmDelayNext = 0;
        inline static bool _retargetEnabled = true;
        inline static std::uint32_t _localShotFrame = 0;
        inline static OpenTK::Mathematics::Vector3 _localShotOrigin{};
        inline static OpenTK::Mathematics::Vector3 _localShotDirection{};
        inline static std::uint32_t _lastRetargetFrame = 0;
        inline static std::array<std::int32_t, Slots> _divergedFrames{};

        inline static std::array<OpenTK::Mathematics::Vector3, Slots> _lastReportPosition{};
        inline static std::array<std::uint32_t, Slots> _lastReportFrame{};
        inline static std::array<bool, Slots> _reportSeen{};
    };
}
