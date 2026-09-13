#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    struct Enemy10Values
    {
        std::uint16_t HealthMax = 0;
        std::uint16_t BeamDamage = 0;
        std::uint16_t SplashDamage = 0;
        std::uint16_t ContactDamage = 0;
        std::int32_t StepDistance1 = 0;
        std::int32_t StepDistance2 = 0;
        std::int32_t StepDistance3 = 0;
        std::int32_t CircleIncrement = 0;
        std::int32_t Unknown18 = 0;
        std::int16_t MinShots = 0;
        std::int16_t MaxShots = 0;
        std::int32_t ScanId = 0;
        std::int32_t Effectiveness = 0;
    };

    class Enemy10Entity : public EnemyInstanceEntity
    {
    public:
        Enemy10Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy10Entity(const Enemy10Entity&) = delete;
        Enemy10Entity& operator=(const Enemy10Entity&) = delete;
        Enemy10Entity(Enemy10Entity&&) = delete;
        Enemy10Entity& operator=(Enemy10Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy10Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy10Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        static const std::array<std::int32_t, 11> _recolors;

        EnemySpawnEntity* const _spawner;
        Enemy10Values _values{};
        std::uint32_t _movementType = 0;
        CollisionVolume _movementVolume{};
        CollisionVolume _homeVolume{};
        float _stepDistance = 0.0F;
        std::uint8_t _nextPattern = 0;
        std::uint8_t _pattern = 0;
        std::uint8_t _finalMoveIndex = 0;
        std::int32_t _stepCount = 0;
        OpenTK::Mathematics::Vector3 _moveTarget{};
        OpenTK::Mathematics::Vector3 _initialPos{};
        OpenTK::Mathematics::Vector3 _aimVector{};
        std::array<OpenTK::Mathematics::Vector3, 16> _movePositions{};
        std::uint8_t _moveIndex = 0;
        std::uint8_t _maxMoveIndex = 0;
        float _circleAngle = 0.0F;

        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = 1000;
        std::uint16_t _shotCount = 0;
        std::uint16_t _shotTimer = 0;

        void StartMovingToward(OpenTK::Mathematics::Vector3 target, float step);
        void StartMovingTowardPosition();
        void MoveInCircle();

        void State0();
        void State1();
        void State2();
        void State3();
        void State4();
        void State5();

        void PlayBeamShotSfx();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
        [[nodiscard]] bool Behavior06();
        [[nodiscard]] bool Behavior07();
        [[nodiscard]] bool Behavior08();
        [[nodiscard]] bool Behavior09();

        void ReachTargetOrReversePattern();
        void ReversePattern();
    };
}
