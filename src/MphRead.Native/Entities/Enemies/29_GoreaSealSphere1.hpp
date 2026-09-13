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
    class Enemy28Entity;

    class Enemy29Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy29Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy29Entity(const Enemy29Entity&) = delete;
        Enemy29Entity& operator=(const Enemy29Entity&) = delete;
        Enemy29Entity(Enemy29Entity&&) = delete;
        Enemy29Entity& operator=(Enemy29Entity&&) = delete;

        [[nodiscard]] std::int32_t Damage() const noexcept;
        [[nodiscard]] std::int32_t DamageTimer() const noexcept;

        ColorRgb Ambient{};
        ColorRgb Diffuse{};

        void Activate();
        void Deactivate();

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy28Entity* _gorea1B = nullptr;
        std::shared_ptr<Node> _attachNode{};

        std::int32_t _damage = 0;
        std::int32_t _damageTimer = 0;
    };
}
