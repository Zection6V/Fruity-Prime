#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead::Mods::Network
{
    class NetHitPrediction final
    {
    public:
        NetHitPrediction() = delete;

        [[nodiscard]] static bool Enabled() noexcept { return _enabled; }
        static void SetEnabled(bool value) noexcept { _enabled = value; }

        [[nodiscard]] static bool MarkerEnabled() noexcept { return _markerEnabled; }
        static void SetMarkerEnabled(bool value) noexcept { _markerEnabled = value; }

        [[nodiscard]] static bool Predicting() noexcept;

        [[nodiscard]] static std::int64_t Predicted() noexcept { return _predicted; }
        [[nodiscard]] static std::int64_t Confirmed() noexcept { return _confirmed; }
        [[nodiscard]] static std::int64_t Denied() noexcept { return _denied; }
        [[nodiscard]] static std::int64_t Unpredicted() noexcept { return _unpredicted; }
        [[nodiscard]] static std::int64_t LethalHeld() noexcept { return _lethalHeld; }
        [[nodiscard]] static std::int64_t DeathsPredicted() noexcept { return _deathsPredicted; }
        [[nodiscard]] static std::int64_t SelfDeathsPredicted() noexcept { return _selfDeathsPredicted; }
        [[nodiscard]] static std::int64_t DeathsUndone() noexcept { return _deathsUndone; }
        [[nodiscard]] static std::int64_t DrainPredicted() noexcept { return _drainPredicted; }
        [[nodiscard]] static std::int64_t SelfPredicted() noexcept { return _selfPredicted; }
        [[nodiscard]] static std::int64_t SelfConfirmed() noexcept { return _selfConfirmed; }

        [[nodiscard]] static bool DeathEnabled() noexcept { return _deathEnabled; }
        static void SetDeathEnabled(bool value) noexcept { _deathEnabled = value; }

        [[nodiscard]] static float MarkerAlpha() noexcept;

        static void Reset();

        [[nodiscard]] static bool Predicts(
            Entities::PlayerEntity& victim,
            Entities::EntityBase* source,
            Entities::DamageFlags flags);

        static void NoteHit(
            Entities::PlayerEntity& victim,
            Entities::PlayerEntity* attacker,
            Entities::DamageFlags flags,
            std::uint32_t& damage);

        [[nodiscard]] static bool Confirm(std::int32_t slot, std::int32_t landed = 1);
        static void ForgetSlot(std::int32_t slot);
        static void NoteRespawn(std::int32_t slot);
        static void NoteDeath(std::int32_t slot);
        static void ForgetPending();
        static void NoteDrain(Entities::PlayerEntity& healer, std::int32_t amount);

        [[nodiscard]] static std::int32_t HealthFor(
            std::int32_t slot, std::int32_t authorityHealth);
        [[nodiscard]] static std::int32_t LocalHealthFor(
            Entities::PlayerEntity& player, std::int32_t authorityHealth);
        [[nodiscard]] static bool HeldDead(std::int32_t slot);

        static void Tick();

        [[nodiscard]] static std::string Describe();

    private:
        static constexpr std::int32_t Slots = Entities::PlayerEntity::SlotCapacity;
        static constexpr std::int32_t PendingFrames = 120;
        static constexpr std::int32_t PendingCapacity = 24;
        static constexpr std::int32_t HealCapacity = 48;
        static constexpr std::int32_t MarkerFrames = 12;

        inline static bool _enabled = true;
        inline static bool _markerEnabled = true;

        inline static std::array<std::array<std::uint32_t, PendingCapacity>, Slots>
            _pendingFrame{};
        inline static std::array<std::array<std::int32_t, PendingCapacity>, Slots>
            _pendingDamage{};
        inline static std::array<std::array<bool, PendingCapacity>, Slots>
            _pendingLethal{};
        inline static std::array<std::int32_t, Slots> _pendingCount{};
        inline static std::array<std::int32_t, Slots> _pendingHead{};

        inline static std::array<std::int32_t, Slots> _shownHealth{};
        inline static std::array<std::uint32_t, Slots> _predictedFrame{};

        inline static std::array<std::uint32_t, HealCapacity> _healFrame{};
        inline static std::array<std::int32_t, HealCapacity> _healAmount{};
        inline static std::int32_t _healCount = 0;
        inline static std::int32_t _healHead = 0;

        inline static std::int64_t _predicted = 0;
        inline static std::int64_t _confirmed = 0;
        inline static std::int64_t _denied = 0;
        inline static std::int64_t _unpredicted = 0;
        inline static std::int64_t _lethalHeld = 0;
        inline static std::int64_t _deathsPredicted = 0;
        inline static std::int64_t _selfDeathsPredicted = 0;
        inline static std::int64_t _deathsUndone = 0;
        inline static std::int64_t _drainPredicted = 0;
        inline static std::int64_t _selfPredicted = 0;
        inline static std::int64_t _selfConfirmed = 0;

        inline static bool _deathEnabled = false;
        inline static std::int32_t _markerTimer = 0;

        [[nodiscard]] static std::int32_t HoldFrames() noexcept;
        [[nodiscard]] static Entities::PlayerEntity* OwnerOf(Entities::EntityBase* source);
        [[nodiscard]] static std::int32_t Debit(std::int32_t slot);
        static void Push(
            std::int32_t slot, std::uint32_t frame, std::int32_t damage, bool lethal);
    };
}
