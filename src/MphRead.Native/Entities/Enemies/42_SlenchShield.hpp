#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities::Enemies
{
    class Enemy41Entity;

    class Enemy42Entity : public EnemyInstanceEntity
    {
    public:
        Enemy42Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy42Entity(const Enemy42Entity&) = delete;
        Enemy42Entity& operator=(const Enemy42Entity&) = delete;
        Enemy42Entity(Enemy42Entity&&) = delete;
        Enemy42Entity& operator=(Enemy42Entity&&) = delete;

        [[nodiscard]] Enemy41Entity* Slench() const noexcept;

        void UpdateScanId(std::int32_t scanId) noexcept;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy41Entity* const _slench;
    };
}
