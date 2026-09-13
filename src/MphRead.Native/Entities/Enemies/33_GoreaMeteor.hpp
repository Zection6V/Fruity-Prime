#pragma once

#include "31_Gorea2.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy33Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy33Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy33Entity(const Enemy33Entity&) = delete;
        Enemy33Entity& operator=(const Enemy33Entity&) = delete;
        Enemy33Entity(Enemy33Entity&&) = delete;
        Enemy33Entity& operator=(Enemy33Entity&&) = delete;

        void InitializePosition(OpenTK::Mathematics::Vector3 position);

        [[nodiscard]] static bool Behavior00(Enemy33Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy33Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy33Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy33Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        PlayerEntity* _target = nullptr;
        std::shared_ptr<Effects::EffectEntry> _effect{};
        OpenTK::Mathematics::Vector3 _effectUp{};
        OpenTK::Mathematics::Vector3 _effectFacing{};

        std::int32_t _shakeTimer = 0;
        float _field1A0 = 0.0F;
        float _field1A4 = 0.0F;
        float _field1AC = 0.0F;
        OpenTK::Mathematics::Vector3 _basePos{};
        float _field1B0 = 0.0F;
        std::int32_t _field1B4 = 0;
        std::int32_t _field1B6 = 0;
        std::int32_t _field1B8 = 0;
        float _field1BC = 0.0F;
        float _field1BE = 0.0F;
        std::int32_t _itemChance1 = 0;
        std::int32_t _itemChance2 = 0;
        std::int32_t _itemChance3 = 0;
        std::int32_t _itemChance4 = 0;
        std::uint8_t _field1C4 = 0;
        std::uint8_t _field1C5 = 0;
        bool _flag = false;

        void UpdateRotation();
        void UpdateSpeed();
        void CheckCollision();
        void CheckHitPlayer();
        void UpdatePosition();
        void Func2140E44();
        void SpawnItemDrop();
        void Explode(std::int32_t effectId);
        void CheckExplosionDamage();

        void State00();
        void State01();
        void State02();
        void State03();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
    };
}
