#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Effects
{
    class EffectEntry;
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy11Entity : public EnemyInstanceEntity
    {
    public:
        Enemy11Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy11Entity(const Enemy11Entity&) = delete;
        Enemy11Entity& operator=(const Enemy11Entity&) = delete;
        Enemy11Entity(Enemy11Entity&&) = delete;
        Enemy11Entity& operator=(Enemy11Entity&&) = delete;

        void Destroy() override;

        [[nodiscard]] static bool Behavior00(Enemy11Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy11Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy11Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy11Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy11Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        CollisionVolume _rangeVolume{};
        CollisionVolume _activeVolume{};
        OpenTK::Mathematics::Vector3 _targetPos{};
        std::int32_t _moveTimer = 0;
        std::shared_ptr<Effects::EffectEntry> _effect{};

        void Die();

        void State0();
        void State1();
        void State2();
        void State3();
        void State4();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
    };
}
