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
    class Enemy16Entity : public EnemyInstanceEntity
    {
    public:
        Enemy16Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        void State0();
        void State1();
        void State2();
        void State3();

        [[nodiscard]] static bool Behavior00(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy16Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy16Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        EnemySpawnEntity* const _spawner;
        std::uint16_t _agitateTimer = 0;
        std::uint16_t _cloudTick = 0;
        std::uint16_t _cloudTimer = 0;
        bool _initialCloudHit = false;

        static constexpr float _nearRadius = 8.0F;
        static constexpr float _cloudRadius = 2.0F;

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
        [[nodiscard]] bool Behavior06();
    };
}
