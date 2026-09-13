#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy35Entity : public EnemyInstanceEntity
    {
    public:
        Enemy35Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy35Entity(const Enemy35Entity&) = delete;
        Enemy35Entity& operator=(const Enemy35Entity&) = delete;
        Enemy35Entity(Enemy35Entity&&) = delete;
        Enemy35Entity& operator=(Enemy35Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy35Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy35Entity* enemy);

    protected:
        EnemySpawnEntity* const _spawner;
        CollisionVolume _homeVolume{};

    private:
        bool _handledRamCol = false;
        bool _ramDamageNeeded = false;
        std::uint16_t _ramDelay = 0;

    protected:
        std::uint16_t _timeInAir = 0;
        bool _airborne = false;
        bool _grounded = false;

    private:
        OpenTK::Mathematics::Vector3 _moveTarget{};

    protected:
        OpenTK::Mathematics::Vector3 _targetVec{};
        OpenTK::Mathematics::Vector3 _moveStart{};

    private:
        float _roamAngleSign = 1.0F;

    protected:
        float _aimAngleStep = 0.0F;
        std::uint16_t _aimSteps = 0;
        float _moveDistSqr = 0.0F;

    private:
        float _moveDistSqrHalf = 0.0F;
        bool _increaseSpeed = false;

    protected:
        float _speedFactor = 0.0F;
        float _speedInc = 0.0F;

    private:
        float _rollSfxAmount = 0.0F;

    protected:
        float _speedIncAmount = 410.0F / 4096.0F * (1.0F / 3.0F) / 2.0F;
        std::uint16_t _aimStepCount = 10 * 2;
        float _minSpeedFactor = 0.1F / 2.0F;
        float _maxSpeedFactor = 0.2F / 2.0F;

        void EnemyInitialize() override;
        virtual void Setup();
        void EnemyProcess() override;
        void UpdateRollSfx(float newAmount, bool grounded);
        [[nodiscard]] virtual bool HandleCollision();
        [[nodiscard]] bool HandleCollision(std::int32_t stateA, std::int32_t stateB);
        void PickRoamTarget();
        void UpdateMoveTarget(OpenTK::Mathematics::Vector3 targetPoint);
        void UpdateSpeed();

    private:
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
    };
}
