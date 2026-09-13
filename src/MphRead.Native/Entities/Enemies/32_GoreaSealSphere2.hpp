#pragma once

#include "31_Gorea2.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class Node;
}

namespace MphRead::Entities::Enemies
{
    class Enemy32Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy32Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy32Entity(const Enemy32Entity&) = delete;
        Enemy32Entity& operator=(const Enemy32Entity&) = delete;
        Enemy32Entity(Enemy32Entity&&) = delete;
        Enemy32Entity& operator=(Enemy32Entity&&) = delete;

        [[nodiscard]] Node* AttachNode() const noexcept;
        [[nodiscard]] std::int32_t Damage() const noexcept;
        void SetDamage(std::int32_t value) noexcept;
        [[nodiscard]] std::int32_t DamageTimer() const noexcept;
        [[nodiscard]] bool Visible() const noexcept;
        [[nodiscard]] bool Targetable() const noexcept;

        void UpdateVisibility();
        void SetDead();

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy31Entity* _gorea2 = nullptr;
        std::shared_ptr<Node> _attachNode{};

        std::int32_t _damage = 0;
        std::int32_t _damageTimer = 0;
        bool _visible = false;
        bool _targetable = false;
    };
}
