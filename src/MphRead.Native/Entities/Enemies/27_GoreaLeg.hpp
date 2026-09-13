#pragma once

#include "24_Gorea1A.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class Node;
}

namespace MphRead::Entities::Enemies
{
    class Enemy27Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy27Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy27Entity(const Enemy27Entity&) = delete;
        Enemy27Entity& operator=(const Enemy27Entity&) = delete;
        Enemy27Entity(Enemy27Entity&&) = delete;
        Enemy27Entity& operator=(Enemy27Entity&&) = delete;

        std::int32_t Index = 0;

        void SetKneeNode(GoreaEnemyEntityBase* parent);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy24Entity* _gorea1A = nullptr;
        std::shared_ptr<Node> _kneeNode{};

        void CheckPlayerCollision(float factor, std::int32_t damage);
    };
}
