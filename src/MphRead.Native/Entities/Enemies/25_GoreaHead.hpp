#pragma once

#include "24_Gorea1A.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class Node;

    namespace Effects
    {
        class EffectEntry;
    }
}

namespace MphRead::Entities::Enemies
{
    class Enemy25Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy25Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy25Entity(const Enemy25Entity&) = delete;
        Enemy25Entity& operator=(const Enemy25Entity&) = delete;
        Enemy25Entity(Enemy25Entity&&) = delete;
        Enemy25Entity& operator=(Enemy25Entity&&) = delete;

        std::int32_t Damage = 0;

        void RespawnFlashEffect();

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        std::shared_ptr<Node> _attachNode{};
        Enemy24Entity* _gorea1A = nullptr;
        std::shared_ptr<Effects::EffectEntry> _flashEffect{};

        void RemoveFlashEffect();
    };
}
