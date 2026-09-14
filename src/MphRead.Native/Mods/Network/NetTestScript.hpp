#pragma once

#include "../../Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::Entities
{
    class Keybind;
    class PlayerControls;
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    enum class TestPhase : std::int32_t
    {
        Idle,
        Walk,
        Jump,
        Turn,
        Shoot,
        SwitchWeapons,
        Charge,
        MorphA,
        AltAttackA,
        MorphB,
        AltAttackB,
        Unmorph,
        Zoom,
        Afflict,
        Duel
    };

    class NetTestScript final
    {
    public:
        NetTestScript() = delete;
        NetTestScript(const NetTestScript&) = delete;
        NetTestScript& operator=(const NetTestScript&) = delete;
        NetTestScript(NetTestScript&&) = delete;
        NetTestScript& operator=(NetTestScript&&) = delete;

        [[nodiscard]] static double PhaseSeconds() noexcept;
        static void SetPhaseSeconds(double value) noexcept;
        [[nodiscard]] static std::int32_t PhaseCount() noexcept;

        [[nodiscard]] static bool Enabled() noexcept;
        static void SetEnabled(bool value) noexcept;

        [[nodiscard]] static float AimDeltaX() noexcept;
        [[nodiscard]] static float AimDeltaY() noexcept;
        [[nodiscard]] static std::int32_t FramesOnTarget() noexcept;

        [[nodiscard]] static TestPhase Phase();

        static void Reset();
        static void ApplyOffline(
            std::shared_ptr<Entities::PlayerEntity> player,
            std::int32_t slot,
            std::int32_t frame);
        static void HoldFire(
            std::shared_ptr<Entities::PlayerEntity> player,
            bool down);
        static void LayBombs(
            std::shared_ptr<Entities::PlayerEntity> player,
            std::int32_t frame);
        static void Rest(
            std::shared_ptr<Entities::PlayerEntity> player,
            bool wantBiped);
        static void WalkForward(std::shared_ptr<Entities::PlayerEntity> player);
        static void Apply(std::shared_ptr<Entities::PlayerEntity> player);

    private:
        [[nodiscard]] static double ReadPhaseSeconds();

        static void Drive(const std::shared_ptr<Entities::PlayerEntity>& player);
        [[nodiscard]] static bool Settled(Entities::PlayerEntity& player);
        [[nodiscard]] static bool Even() noexcept;
        static void MorphOrShoot(
            Entities::PlayerEntity& player,
            Entities::PlayerControls& controls,
            bool morphing);
        static void AltAttackOrShoot(
            Entities::PlayerControls& controls,
            bool attacking,
            bool onTarget);
        static void Clear(Entities::PlayerControls& controls);
        static void Finish(
            Entities::PlayerEntity& player,
            Entities::PlayerControls& controls);
        [[nodiscard]] static bool AimAt(
            Entities::PlayerEntity& player,
            const std::shared_ptr<Entities::PlayerEntity>& target);
        static void Square(Entities::PlayerControls& controls);
        static void Duel(
            Entities::PlayerEntity& player,
            Entities::PlayerControls& controls,
            const std::shared_ptr<Entities::PlayerEntity>& target,
            bool onTarget,
            bool charged = false);
        [[nodiscard]] static std::shared_ptr<Entities::PlayerEntity> FindTarget(
            const std::shared_ptr<Entities::PlayerEntity>& self);
        static void Hold(Entities::Keybind& bind, bool down);

        static constexpr float TurnRate = 6.0F;
        static constexpr float FiringCone = 6.0F;
        static constexpr float PreferredRange = 4.0F;

        static double _phaseSeconds;
        inline static const std::array<TestPhase, 15> _order{
            TestPhase::Idle,
            TestPhase::Walk,
            TestPhase::Jump,
            TestPhase::Turn,
            TestPhase::Shoot,
            TestPhase::SwitchWeapons,
            TestPhase::Charge,
            TestPhase::MorphA,
            TestPhase::AltAttackA,
            TestPhase::MorphB,
            TestPhase::AltAttackB,
            TestPhase::Unmorph,
            TestPhase::Zoom,
            TestPhase::Afflict,
            TestPhase::Duel
        };

        inline static bool _enabled = false;
        inline static std::int32_t _frame = 0;
        inline static std::int32_t _stuckFrames = 0;
        inline static bool _stuckDirection = false;
        static OpenTK::Mathematics::Vector3 _lastPosition;

        inline static float _aimDeltaX = 0.0F;
        inline static float _aimDeltaY = 0.0F;
        inline static std::int32_t _framesOnTarget = 0;

        inline static std::int32_t _offlineSlot = -1;
        inline static std::vector<bool> _wasDown{};
        inline static std::int32_t _releaseFrames = 0;
    };
}
