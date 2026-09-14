#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities::Enemies
{
    class Enemy41Entity;
    class Enemy45Entity;

    enum class SynapseState : std::uint8_t
    {
        Initial = 0,
        Appear = 1,
        Idle = 2,
        Damaged = 3,
        Dying = 4,
        Dead = 5
    };

    struct Enemy44Values
    {
        std::uint16_t ScanId = 0;
        std::uint16_t Health = 0;
        std::uint16_t HealTimer = 0;
        std::uint16_t ReappearTimer = 0;
        std::int32_t ColRadius = 0;
        std::uint16_t Magic = 0;
        std::uint16_t PaddingE = 0;
    };

    class Enemy44Entity : public EnemyInstanceEntity
    {
    public:
        Enemy44Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy44Entity(const Enemy44Entity&) = delete;
        Enemy44Entity& operator=(const Enemy44Entity&) = delete;
        Enemy44Entity(Enemy44Entity&&) = delete;
        Enemy44Entity& operator=(Enemy44Entity&&) = delete;

        [[nodiscard]] SynapseState State() const noexcept;

        void ChangeState(SynapseState state);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy41Entity* const _slench;
        ModelInstance* _model = nullptr;
        std::int32_t _index = 0;
        std::uint16_t _healthForTurretUpdate = 0;
        std::int32_t _timer = 0;

        [[nodiscard]] Enemy44Values GetValues() const;
        [[nodiscard]] Enemy44Values GetPhaseValues() const;
        void UpdateCollisionVolume(float radius);
        void SetTurretActive(bool activate);
        [[nodiscard]] Enemy45Entity* FindTurret();
    };
}
