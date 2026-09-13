#pragma once

#include "24_Gorea1A.hpp"

#include <cstdint>

namespace MphRead::Entities::Enemies
{
    class Enemy30Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy30Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy30Entity(const Enemy30Entity&) = delete;
        Enemy30Entity& operator=(const Enemy30Entity&) = delete;
        Enemy30Entity(Enemy30Entity&&) = delete;
        Enemy30Entity& operator=(Enemy30Entity&&) = delete;

        Enemy28Entity* Gorea1B = nullptr;
        std::int32_t Index = 0;
        OpenTK::Mathematics::Vector3 Field174{};
        std::int32_t Field184 = 0;
        std::int32_t State = 0;

        void Explode();
        void SetSpeed(OpenTK::Mathematics::Vector3 speed);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        void DieAndSpawnEffect(std::int32_t effectId);

        EnemySpawnEntity* const _spawner;
    };
}
