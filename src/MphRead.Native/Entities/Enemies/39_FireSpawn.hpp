#pragma once

#include "../../Formats/Culling.hpp"
#include "../../Formats/Effects.hpp"
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
}

namespace MphRead::Entities::Enemies
{
    class Enemy50Entity;

    struct Enemy39Values
    {
        std::uint16_t HealthMax = 0;
        std::uint16_t BeamDamage = 0;
        std::uint16_t SplashDamage = 0;
        std::uint16_t ContactDamage = 0;
        std::int16_t Unused8 = 0;
        std::uint16_t AttackDelay = 0;
        std::uint16_t AttackCountMin = 0;
        std::uint16_t AttackCountMax = 0;
        std::uint16_t DiveTimerMin = 0;
        std::uint16_t DiveTimerMax = 0;
        std::int32_t Unused14 = 0;
        std::int16_t Unused18 = 0;
        std::int16_t ScanId = 0;
        std::int32_t Effectiveness = 0;
    };

    class Enemy39Entity : public EnemyInstanceEntity
    {
    public:
        Enemy39Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy39Entity(const Enemy39Entity&) = delete;
        Enemy39Entity& operator=(const Enemy39Entity&) = delete;
        Enemy39Entity(Enemy39Entity&&) = delete;
        Enemy39Entity& operator=(Enemy39Entity&&) = delete;

        [[nodiscard]] Enemy39Values Values() const noexcept;

        [[nodiscard]] static bool Behavior0(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior1(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior2(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior3(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior4(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior5(Enemy39Entity* enemy);
        [[nodiscard]] static bool Behavior6(Enemy39Entity* enemy);

        void Destroy() override;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        static const std::array<std::int32_t, 11> _recolors;

        EnemySpawnEntity* const _spawner;
        std::int32_t _animFrameCount = 0;
        CollisionVolume _activeVolume{};
        CollisionVolume _locationVolume{};
        std::array<std::shared_ptr<EquipInfo>, 2> _equipInfo{};
        std::int32_t _wristId = 1;
        std::int32_t _tangibilityTimer = 0;
        std::int32_t _attackDelay = 0;
        std::uint32_t _attackCount = 0;
        std::uint32_t _diveTimer = 0;
        std::int32_t _surfaceDirection = 1;
        std::shared_ptr<Effects::EffectEntry> _effectEntry{};
        std::shared_ptr<Node> _wristNodeL{};
        std::shared_ptr<Node> _wristNodeR{};
        std::array<OpenTK::Mathematics::Vector3, 2> _wristPos{};
        std::shared_ptr<Enemy50Entity> _hitZone{};
        std::int32_t _ammo0 = 1000;
        std::int32_t _ammo1 = 1000;
        Enemy39Values _values{};

        void State0();
        void State1();
        void State2();
        void State3();
        void State4();
        void CreateEffect();
        void State5();
        [[nodiscard]] bool Behavior0();
        void ChooseSurfaceLocation();
        [[nodiscard]] bool Behavior1();
        [[nodiscard]] bool Behavior2();
        [[nodiscard]] bool Behavior3();
        [[nodiscard]] bool Behavior4();
        [[nodiscard]] bool Behavior5();
        [[nodiscard]] bool Behavior6();
        [[nodiscard]] bool StartSubmerge();
    };
}
