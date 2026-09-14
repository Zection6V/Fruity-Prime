#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
}

namespace MphRead::Entities
{
    class ForceFieldEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy49Entity : public EnemyInstanceEntity
    {
    public:
        Enemy49Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy49Entity(const Enemy49Entity&) = delete;
        Enemy49Entity& operator=(const Enemy49Entity&) = delete;
        Enemy49Entity(Enemy49Entity&&) = delete;
        Enemy49Entity& operator=(Enemy49Entity&&) = delete;

        void LockHit(EntityBase* source);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        OpenTK::Mathematics::Vector3 _vec1 = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _vec2 = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _fieldPosition = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _targetPosition = OpenTK::Mathematics::Vector3::Zero;
        ForceFieldEntity* const _forceField;
        std::uint8_t _shotFrames = 0;
        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = -1;
        OpenTK::Mathematics::Vector3 _ownSpeed = OpenTK::Mathematics::Vector3::Zero;

        void ClearEffectiveness();
        void SetEffectiveness(BeamType type, Effectiveness effectiveness);
    };
}
