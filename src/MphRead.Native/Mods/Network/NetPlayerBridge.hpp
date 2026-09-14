#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"
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
        inline static std::int64_t NodeLookupsUnresolved = 0;

        [[nodiscard]] static std::int64_t RejectedUpdates() noexcept { return _rejectedUpdates; }
        [[nodiscard]] static std::int64_t Snaps() noexcept { return _snaps; }
        [[nodiscard]] static float WorstSnap() noexcept { return _worstSnap; }

        [[nodiscard]] static std::string FormSaidByAuthority();
        [[nodiscard]] static OpenTK::Mathematics::Vector3 InFormFor(
            Entities::PlayerEntity& player,
            OpenTK::Mathematics::Vector3 position,
            bool measuredInAlt);

        static void RecordPresses(Entities::PlayerEntity& player);
        [[nodiscard]] static IntentPacket CaptureIntent(Entities::PlayerEntity& player);
        static void ApplyIntent(Entities::PlayerEntity& player, const IntentPacket& intent);
        static void ApplyState(
            Entities::PlayerEntity& player, const PlayerState& state, bool isLocal);

        static void NoteRoomChanged();
        static void Reset();
        static void ForgetSlot(std::int32_t slot);

        static void ApplyReportedPosition(
            Entities::PlayerEntity& player, const IntentPacket& intent);
        static void RestoreReportedPosition(
            Entities::PlayerEntity& player, const IntentPacket& intent);

    private:
        static constexpr std::int32_t FormGraceFrames = 90;
        static constexpr float SnapDistance = 15.0F;
        [[maybe_unused]] static constexpr float CatchUpRate = 0.35F;
        [[maybe_unused]] static constexpr float FastCatchUpRate = 0.6F;
        [[maybe_unused]] static constexpr float FastCatchUpAbove = 3.0F;
        static constexpr float DesyncDistance = 30.0F;
        static constexpr float MaxReportedSpeed = 5.0F;
        static constexpr float PositionLimit = 100000.0F;
        static constexpr std::int32_t DivergedFramesBeforeCorrecting = 60;
        static constexpr std::int32_t StaleAfterSpawnFrames = 120;
        static constexpr IntentButtons PressedButtons = ~(
            IntentButtons::ZoomedState | IntentButtons::AltFormState
            | IntentButtons::InPlayState | IntentButtons::SpectatingState
            | IntentButtons::ReadyState);

        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>
            _formMismatch{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity>
            _authoritySpawned{};
        inline static std::array<std::uint8_t, Entities::PlayerEntity::SlotCapacity>
            _formSaid{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>
            _formAttempts{};

        inline static std::int64_t _rejectedUpdates = 0;
        inline static std::int64_t _snaps = 0;
        inline static float _worstSnap = 0.0F;

        inline static std::array<std::uint32_t, IntentPacket::PressHistory>
            _pressHistory{};
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity>
            _lastPressFrame{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity>
            _pressSeen{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>
            _divergedFrames{};
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity>
            _spawnIntentFrame{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity>
            _wasInPlay{};
        inline static std::array<std::int32_t, Entities::PlayerEntity::SlotCapacity>
            _staleFrames{};
        inline static std::array<OpenTK::Mathematics::Vector3,
            Entities::PlayerEntity::SlotCapacity> _lastReportPosition{};
        inline static std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity>
            _lastReportFrame{};
        inline static std::array<bool, Entities::PlayerEntity::SlotCapacity>
            _reportSeen{};

        [[nodiscard]] static OpenTK::Mathematics::Vector3 InForm(
            Entities::PlayerEntity& player,
            OpenTK::Mathematics::Vector3 position,
            bool measuredInAlt);
        [[nodiscard]] static bool Sane(OpenTK::Mathematics::Vector3 value) noexcept;
        [[nodiscard]] static IntentButtons MissedPresses(
            std::int32_t slot, const IntentPacket& intent);
        static void Set(Entities::Keybind& bind, bool down, bool pressed = false);
        static void ApplyAfflictions(Entities::PlayerEntity& player, PlayerState state);
        static void ApplyForm(Entities::PlayerEntity& player, bool altForm);
        [[nodiscard]] static bool Diverged(
            Entities::PlayerEntity& player, const PlayerState& state, std::int32_t slot);
        [[nodiscard]] static bool StaleSinceSpawn(
            Entities::PlayerEntity& player, const IntentPacket& intent);
        static void NoteReportedVelocity(
            Entities::PlayerEntity& player,
            OpenTK::Mathematics::Vector3 reported,
            std::uint32_t frame);
        [[nodiscard]] static bool FrozenInPlace(Entities::PlayerEntity& player);
        static void Move(
            Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position);
    };
}
