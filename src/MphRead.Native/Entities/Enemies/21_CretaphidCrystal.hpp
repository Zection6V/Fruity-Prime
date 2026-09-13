#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
    class Node;
}

namespace MphRead::Entities::Enemies
{
    class Enemy19Entity;

    class Enemy21Entity : public EnemyInstanceEntity
    {
    public:
        Enemy21Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy21Entity(const Enemy21Entity&) = delete;
        Enemy21Entity& operator=(const Enemy21Entity&) = delete;
        Enemy21Entity(Enemy21Entity&&) = delete;
        Enemy21Entity& operator=(Enemy21Entity&&) = delete;

        void SetUp(std::shared_ptr<Node> attachNode, std::int32_t scanId,
            std::uint32_t effectiveness, std::uint16_t health,
            OpenTK::Mathematics::Vector3 position);
        void SpawnBeam(std::uint16_t damage);

    protected:
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy19Entity* const _cretaphid;
        std::shared_ptr<Node> _attachNode{};
        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = 1000;
    };
}
