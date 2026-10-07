#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
#include "FormReconciliation.hpp"
#include "NetProtocol.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace MphRead::Entities
{
    class Keybind;
    class PlayerControls;
}

namespace MphRead::Mods::Network
{
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

        static void ApplyState(Entities::PlayerEntity& player, const PlayerState& state, bool isLocal);
        [[nodiscard]] static FormCorrection ReconcileForm(std::int32_t slot, std::uint32_t frame, bool desiredAlt,
            bool actualAlt, bool morphing, bool unmorphing, std::int32_t ping);

        static void NoteRoomChanged();
        static void Reset();
        static void ForgetSlot(std::int32_t slot);
        // The victim's view of a remote shot. A remote player's aim is the
        // one they had on their own screen, where this player stood a round
        // trip ago; drawn as is, the shot sails past and the damage lands
        // anyway. When that aim was on this player in the world the shooter
        // was looking at, it is turned onto where this player is now, so the
        // shot that hits is also the shot that is seen hitting.
        [[nodiscard]] static OpenTK::Mathematics::Vector3 RetargetAtLocal(const Entities::PlayerEntity& shooter,
            OpenTK::Mathematics::Vector3 origin, OpenTK::Mathematics::Vector3 aim, std::uint32_t ackFrame,
            OpenTK::Mathematics::Vector3 reportedPosition);
        [[nodiscard]] static std::int64_t AimsRetargeted() noexcept { return _aimsRetargeted; }
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
        inline static bool _retargetEnabled = true;
        inline static std::uint32_t _lastRetargetFrame = 0;
        inline static std::array<std::int32_t, Slots> _divergedFrames{};

        inline static std::array<OpenTK::Mathematics::Vector3, Slots> _lastReportPosition{};
        inline static std::array<std::uint32_t, Slots> _lastReportFrame{};
        inline static std::array<bool, Slots> _reportSeen{};
    };
}
