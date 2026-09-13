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
    class Enemy12Entity : public EnemyInstanceEntity
    {
    public:
        enum class GeemerAnim : std::int32_t
        {
            None = -1,
            Retract = 0,
            Extend = 1,
            WiggleRetracted = 2,
            WiggleExtended = 3
        };

        Enemy12Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy12Entity(const Enemy12Entity&) = delete;
        Enemy12Entity& operator=(const Enemy12Entity&) = delete;
        Enemy12Entity(Enemy12Entity&&) = delete;
        Enemy12Entity& operator=(Enemy12Entity&&) = delete;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        EnemySpawnEntity* const _spawner;
        OpenTK::Mathematics::Vector3 _direction{};
        float _angleInc = 0.0F;
        float _curAngle = 0.0F;
        float _maxAngle = 0.0F;
        float _angleCos = 0.0F;
        OpenTK::Mathematics::Vector3 _intendedDir{};
        OpenTK::Mathematics::Vector3 _field19C{};
        OpenTK::Mathematics::Vector3 _field1A8{};
        std::int32_t _volumeCheckDelay = 0;
        bool _seekingVolume = false;
        CollisionVolume _homeVolume{};
        std::uint16_t _extendTimer = 0;

        void SetAnimation(GeemerAnim anim, AnimFlags flags = AnimFlags::None);
    };
}
