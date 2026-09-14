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
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    struct Enemy45Values
    {
        std::uint16_t Health = 0;
        std::uint16_t Damage = 0;
        std::uint16_t Unused4 = 0;
        std::uint16_t ContactDamage = 0;
        std::uint16_t ShotCooldown = 0;
        std::uint16_t SalvoCooldown = 0;
        std::uint16_t MinShots = 0;
        std::uint16_t MaxShots = 0;
        std::int32_t ScanId = 0;
        std::int32_t Effectiveness = 0;
    };

    class Enemy45Entity : public EnemyInstanceEntity
    {
    public:
        Enemy45Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy45Entity(const Enemy45Entity&) = delete;
        Enemy45Entity& operator=(const Enemy45Entity&) = delete;
        Enemy45Entity(Enemy45Entity&&) = delete;
        Enemy45Entity& operator=(Enemy45Entity&&) = delete;

        [[nodiscard]] std::int32_t Index() const noexcept;
        [[nodiscard]] std::int32_t GetMaxFrameCount();

        void HandleMessage(MessageInfo info) override;

        [[nodiscard]] static bool Behavior00(Enemy45Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy45Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy45Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy45Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy45Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        std::int32_t _index = 0;
        CollisionVolume _volume{};
        ModelInstance* _model = nullptr;
        Enemy45Values _values{};

        std::shared_ptr<EquipInfo> _equipInfo{};
        std::int32_t _ammo = 1000;
        std::int32_t _shotCooldown = 0;
        std::int32_t _shotsRemaining = 0;
        std::int32_t _salvoCooldown = 0;

        std::int32_t _animInterval = 1;
        std::int32_t _animDelayTimer = 1;
        bool _animReverse = false;
        bool _animating = false;
        std::int32_t _animFrameCount = 0;

        void UpdateShotCount();
        void UpdateAnimationFrame();
        void SetAnimation();
        void SetAnimationReverse();
        void State0();
        void State2();
        void SpawnChargeEffect();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
    };
}
