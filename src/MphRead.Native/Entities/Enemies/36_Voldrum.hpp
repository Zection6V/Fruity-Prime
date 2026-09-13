#pragma once

#include "35_Voldrum.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
}

namespace MphRead::Entities::Enemies
{
    struct Enemy36Values
    {
        std::uint16_t HealthMax = 0;
        std::uint16_t BeamDamage = 0;
        std::uint16_t SplashDamage = 0;
        std::uint16_t ContactDamage = 0;
        std::int32_t MinSpeedFactor = 0;
        std::int32_t MaxSpeedFactor = 0;
        std::uint16_t DoubleSpeedSteps = 0;
        std::uint16_t AimSteps = 0;
        std::uint16_t DelayTime = 0;
        std::uint16_t ShotTime = 0;
        std::uint16_t MinShots = 0;
        std::uint16_t MaxShots = 0;
        std::int32_t JumpSpeed = 0;
        std::int32_t RangeMaxCosine = 0;
        std::uint16_t SpeedSteps = 0;
        std::int16_t ScanId = 0;
        std::int32_t Effectiveness = 0;
    };

    class Enemy36Entity : public Enemy35Entity
    {
    public:
        Enemy36Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy36Entity(const Enemy36Entity&) = delete;
        Enemy36Entity& operator=(const Enemy36Entity&) = delete;
        Enemy36Entity(Enemy36Entity&&) = delete;
        Enemy36Entity& operator=(Enemy36Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy36Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy36Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy36Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy36Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy36Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy36Entity* enemy);

    protected:
        void Setup() override;
        void EnemyProcess() override;
        [[nodiscard]] bool HandleCollision() override;

    private:
        static const std::array<std::int32_t, 11> _recolors;

        Enemy36Values _values{};
        std::shared_ptr<EquipInfo> _equipInfo1{};
        std::shared_ptr<EquipInfo> _equipInfo2{};
        std::int32_t _ammo1 = 1000;
        std::int32_t _ammo2 = 1000;
        std::uint16_t _delayTimer = 0;
        std::uint16_t _shotCount = 0;
        std::uint16_t _shotTimer = 0;

        void State0();
        void State1();
        void UpdateFacing();
        void State2();
        void State3();
        void State4();
        void State5();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
    };
}
