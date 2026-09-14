#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
    class ModelInstance;
    enum class Movie : std::int32_t;

    namespace Effects
    {
        class EffectEntry;
    }
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy42Entity;
    class Enemy44Entity;

    enum class SlenchFlags : std::uint16_t
    {
        None = 0x0,
        EyeClosed = 0x1,
        Rolling = 0x2,
        Floating = 0x4,
        Bouncy = 0x8,
        PatternFlip1 = 0x10,
        PatternFlip2 = 0x20,
        Detached = 0x40,
        TargetingPlayer = 0x80,
        Vulnerable = 0x100,
        Wobbling = 0x200,
        Unused10 = 0x400,
        Unused11 = 0x800,
        Unused12 = 0x1000,
        Unused13 = 0x2000,
        Unused14 = 0x4000,
        Unused15 = 0x8000
    };

    [[nodiscard]] constexpr SlenchFlags operator|(SlenchFlags left, SlenchFlags right) noexcept
    {
        return static_cast<SlenchFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SlenchFlags operator&(SlenchFlags left, SlenchFlags right) noexcept
    {
        return static_cast<SlenchFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SlenchFlags operator^(SlenchFlags left, SlenchFlags right) noexcept
    {
        return static_cast<SlenchFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SlenchFlags operator~(SlenchFlags value) noexcept
    {
        return static_cast<SlenchFlags>(static_cast<std::uint16_t>(
            ~static_cast<std::uint16_t>(value)));
    }

    constexpr SlenchFlags& operator|=(SlenchFlags& left, SlenchFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr SlenchFlags& operator&=(SlenchFlags& left, SlenchFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr SlenchFlags& operator^=(SlenchFlags& left, SlenchFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class SlenchState : std::uint8_t
    {
        Initial = 0,
        Intro = 1,
        ShieldRaise = 2,
        Idle = 3,
        ShootTear = 4,
        ShieldLower = 5,
        Return = 6,
        Attach = 7,
        Detach = 8,
        RollingDone = 9,
        Roam = 10,
        SlamReady = 11,
        Slam = 12,
        SlamReturn = 13,
        Dead = 14
    };

    struct Enemy41Values
    {
        std::uint16_t ScanId1 = 0;
        std::uint16_t ScanId2 = 0;
        std::int32_t AngleIncrement1 = 0;
        std::int32_t Health = 0;
        std::int32_t AngleIncrement2 = 0;
        std::uint16_t MinStaticShotTimer = 0;
        std::uint16_t MaxStaticShotTimer = 0;
        std::int16_t StaticShotCooldown = 0;
        std::uint8_t StaticShotCount = 0;
        std::uint8_t Padding17 = 0;
        std::int32_t AngleIncrement3 = 0;
        std::int32_t MoveIncrement1 = 0;
        std::int32_t MoveIncrement2 = 0;
        std::int32_t AngleIncrement4 = 0;
        std::int32_t RoamTime = 0;
        std::int32_t MoveIncrement3 = 0;
        std::int32_t RollTime = 0;
        std::int32_t FloatingAngleInc = 0;
        std::int32_t RollingAngleInc = 0;
        std::int32_t FloatingSpeed = 0;
        std::int32_t RollingSpeed = 0;
        std::int32_t AngleIncrement5 = 0;
        std::int32_t SlamRange = 0;
        std::int32_t MoveIncrement4 = 0;
        std::int32_t MoveIncrement5 = 0;
        std::uint16_t SlamDelay = 0;
        std::uint8_t WobbleCycles = 0;
        std::uint8_t WobbleRotInc = 0;
        std::int32_t MaxWobbleDist = 0;
        std::uint16_t Magic = 0;
        std::uint16_t Padding5E = 0;
    };

    class Enemy41Entity : public EnemyInstanceEntity
    {
    public:
        Enemy41Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy41Entity(const Enemy41Entity&) = delete;
        Enemy41Entity& operator=(const Enemy41Entity&) = delete;
        Enemy41Entity(Enemy41Entity&&) = delete;
        Enemy41Entity& operator=(Enemy41Entity&&) = delete;

        [[nodiscard]] Enemies::SlenchFlags SlenchFlags() const noexcept;
        [[nodiscard]] std::int32_t Subtype() const noexcept;
        [[nodiscard]] std::int32_t Phase() const noexcept;
        [[nodiscard]] float ShieldOffset() const noexcept;
        [[nodiscard]] std::int32_t SynapseIndex() const noexcept;

        [[nodiscard]] bool CanSynapsesRespawn();
        [[nodiscard]] bool ShieldTakeDamage(EntityBase* source);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        static constexpr std::uint16_t _synapseCount = 3;

        EnemySpawnEntity* const _spawner;
        Enemies::SlenchFlags _slenchFlags = Enemies::SlenchFlags::None;
        std::int32_t _subtype = 0;
        std::int32_t _phase = 0;
        std::uint8_t _stateAfterSlam = 0;

        float _targetAngle = 0.0F;
        float _targetX = 0.0F;
        float _targetZ = 0.0F;
        OpenTK::Mathematics::Vector3 _targetHorizontal{};
        std::uint16_t _recoilTimer = 0;
        std::uint16_t _slamTimer = 0;
        float _wobbleAngle = 0.0F;
        float _shieldOffset = 0.0F;
        OpenTK::Mathematics::Vector3 _startPos{};
        OpenTK::Mathematics::Vector3 _detachedPosition{};
        OpenTK::Mathematics::Vector3 _detachedFacing{};
        float _floatBaseY = 0.0F;
        float _patternAngle = 0.0F;
        float _dropSpeed = 0.0F;
        OpenTK::Mathematics::Vector3 _destVec1{};
        OpenTK::Mathematics::Vector3 _destVec2{};

        std::int32_t _staticShotCounter = 0;
        std::int32_t _wobbleTimer = 0;
        std::int32_t _staticShotCooldown = 0;
        std::int32_t _roamTimer = 0;
        std::int32_t _staticShotTimer = 0;
        std::int32_t _deathTimer = 0;
        std::int32_t _rollTimer = 0;
        bool _hitFloor = false;

        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = 1000;
        std::int32_t _shotCooldown = 0;

        ModelInstance* _model = nullptr;
        std::shared_ptr<Enemy42Entity> _shield{};
        std::array<std::shared_ptr<Enemy44Entity>, _synapseCount> _synapses{};
        std::int32_t _synapseIndex = 0;

        std::shared_ptr<Effects::EffectEntry> _shotEffect{};
        std::shared_ptr<Effects::EffectEntry> _shieldEffect1{};
        std::shared_ptr<Effects::EffectEntry> _shieldEffect2{};
        std::shared_ptr<Effects::EffectEntry> _damageEffect{};

        static const std::array<float, 10> _recoilLut;
        static const std::array<Movie, 4> _deathMovieIds;

        [[nodiscard]] Enemy41Values GetValues();
        [[nodiscard]] Enemy41Values GetPhaseValues();
        [[nodiscard]] SlenchState State() const noexcept;

        void UpdateFacing();
        void ChangeState(SlenchState state);
        void UpdateScanId(std::int32_t scanId);
        void OpenEye();
        void CloseEye();
        void DropToFloor(float step);
        [[nodiscard]] bool CheckCollision(
            OpenTK::Mathematics::Vector3 vec, float dist);
        void SetRecoilTargetVecs();
        void ProcessRecoil();
        [[nodiscard]] bool IsStaticState() noexcept;
        [[nodiscard]] bool AreAllSynapsesDead();
        [[nodiscard]] bool SetUpSlam(OpenTK::Mathematics::Vector3 target);
        [[nodiscard]] bool CheckPosAgainstCurrent(
            OpenTK::Mathematics::Vector3 pos) noexcept;
        [[nodiscard]] bool MoveToPosition(
            OpenTK::Mathematics::Vector3 position, float increment);
        [[nodiscard]] bool RotateToTarget(
            OpenTK::Mathematics::Vector3 target, float increment);
        [[nodiscard]] bool RotateToTargetHorizontal(
            OpenTK::Mathematics::Vector3 target, float increment);
    };
}
