#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
    class Node;
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
    class PlayerEntity;
}

namespace MphRead::Entities::Enemies
{
    struct Enemy18Values
    {
        std::uint16_t HealthMax = 0;
        std::uint16_t BeamDamage = 0;
        std::uint16_t SplashDamage = 0;
        std::uint16_t ContactDamage = 0;
        std::int32_t MinAngleY = 0;
        std::int32_t MaxAngleY = 0;
        std::int32_t AngleIncY = 0;
        std::int32_t MinAngleX = 0;
        std::int32_t MaxAngleX = 0;
        std::int32_t AngleIncX = 0;
        std::uint16_t ShotCooldown = 0;
        std::uint16_t DelayTime = 0;
        std::uint16_t MinShots = 0;
        std::uint16_t MaxShots = 0;
        std::int32_t Unused28 = 0;
        std::uint16_t Unused2C = 0;
        std::uint16_t Unused2E = 0;
        std::int32_t ShotOffset = 0;
        std::int32_t ScanId = 0;
        std::int32_t Effectiveness = 0;
    };

    class Enemy18Entity : public EnemyInstanceEntity
    {
    public:
        Enemy18Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy18Entity(const Enemy18Entity&) = delete;
        Enemy18Entity& operator=(const Enemy18Entity&) = delete;
        Enemy18Entity(Enemy18Entity&&) = delete;
        Enemy18Entity& operator=(Enemy18Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy18Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy18Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy18Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy18Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy18Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy18Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        static const std::array<std::int32_t, 11> _recolors;

        EnemySpawnEntity* const _spawner;
        Enemy18Values _values{};
        std::shared_ptr<PlayerEntity> _target{};
        CollisionVolume _rangeVolume{};

        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = 1000;
        std::uint16_t _shotCount = 0;
        std::uint16_t _shotTimer = 0;
        std::uint16_t _delayTimer = 0;

        OpenTK::Mathematics::Vector3 _initialFacing{};
        OpenTK::Mathematics::Vector3 _aimVec{};
        float _angleIncYSign = 1.0F;
        float _angleIncXSign = 1.0F;
        float _angleY = 0.0F;
        float _angleX = 0.0F;
        OpenTK::Mathematics::Vector3 _targetVec{};
        OpenTK::Mathematics::Vector3 _crossVec{};
        float _aimAngleStep = 0.0F;
        std::uint16_t _aimSteps = 0;

        std::shared_ptr<Node> _rotNode{};
        OpenTK::Mathematics::Vector3 _rotNodePos{};

        void ContactDamagePlayer();
        void State0();
        void State1();
        void UpdateAimVec();
        void State2();
        void State3();
        void State4();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
    };
}
