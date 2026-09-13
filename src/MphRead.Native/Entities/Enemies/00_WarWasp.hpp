#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy00Entity : public EnemyInstanceEntity
    {
    public:
        Enemy00Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy00Entity(const Enemy00Entity&) = delete;
        Enemy00Entity& operator=(const Enemy00Entity&) = delete;
        Enemy00Entity(Enemy00Entity&&) = delete;
        Enemy00Entity& operator=(Enemy00Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy00Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy00Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        std::uint32_t _movementType = 0;
        CollisionVolume _movementVolume{};
        CollisionVolume _homeVolume{};
        float _stepDistance = 0.0F;
        std::uint8_t _nextPattern = 0;
        std::uint8_t _pattern = 0;
        std::uint8_t _finalMoveIndex = 0;
        std::int32_t _stepCount = 0;
        std::uint16_t _attackDelay = 0;
        OpenTK::Mathematics::Vector3 _attackTarget{};
        OpenTK::Mathematics::Vector3 _moveTarget{};
        OpenTK::Mathematics::Vector3 _initialPos{};
        std::array<OpenTK::Mathematics::Vector3, 16> _movePositions{};
        std::uint8_t _moveIndex = 0;
        std::uint8_t _maxMoveIndex = 0;
        float _circleAngle = 0.0F;

        void StartMovingToward(OpenTK::Mathematics::Vector3 target, float step);
        void StartMovingTowardPosition();
        void MoveInCircle();

        void State0();
        void State1();
        void State2();
        void State3();
        void State4();
        void State5();
        void State6();

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
        [[nodiscard]] bool Behavior10();

        void ReachTargetOrReversePattern();
        void ReversePattern();
    };
}
