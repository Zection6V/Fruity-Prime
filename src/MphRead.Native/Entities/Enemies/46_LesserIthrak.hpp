#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class Material;
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy50Entity;

    class Enemy46Entity : public EnemyInstanceEntity
    {
    public:
        Enemy46Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy46Entity(const Enemy46Entity&) = delete;
        Enemy46Entity& operator=(const Enemy46Entity&) = delete;
        Enemy46Entity(Enemy46Entity&&) = delete;
        Enemy46Entity& operator=(Enemy46Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior13(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior14(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior15(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior16(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior17(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior18(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior19(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior20(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior21(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior22(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior23(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior24(Enemy46Entity* enemy);
        [[nodiscard]] static bool Behavior25(Enemy46Entity* enemy);

    protected:
        EnemySpawnEntity* const _spawner;

    private:
        CollisionVolume _homeVolume{};
        CollisionVolume _rangeVolume{};
        CollisionVolume _warnVolume{};
        std::shared_ptr<Enemy50Entity> _hitZone{};

        OpenTK::Mathematics::Vector3 _moveTarget{};
        OpenTK::Mathematics::Vector3 _moveStart{};
        float _dropAngleSign = 1.0F;
        float _recoilAngleSign = 1.0F;
        float _moveDistSqr = 0.0F;

        OpenTK::Mathematics::Vector3 _targetVec{};
        float _aimAngleStep = 0.0F;
        std::uint16_t _stepCount = 0;

        std::uint16_t _delayTimer = 0;
        std::uint16_t _moveTimer = 0;
        OpenTK::Mathematics::Vector3 _acceleration{};
        bool _wallCol = false;
        bool _groundCol = false;
        bool _reachingTarget = false;

        const float _stepDistance = 0.5F / 2.0F;
        const float _accelSteps = 10.0F * 2.0F;

    protected:
        Material* _mouthMaterial = nullptr;

        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

        void Setup(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 facing, std::int32_t effectiveness,
            RawCollisionVolume hurtVolume, RawCollisionVolume volume1,
            RawCollisionVolume volume2, RawCollisionVolume volume3);

        virtual void CallSubroutine();
        virtual void UpdateMouthMaterial();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
        [[nodiscard]] bool Behavior06();
        [[nodiscard]] bool Behavior07();
        [[nodiscard]] bool Behavior08();
        [[nodiscard]] bool Behavior09();
        [[nodiscard]] bool Behavior10();
        [[nodiscard]] bool Behavior11();
        [[nodiscard]] bool Behavior12();
        [[nodiscard]] bool Behavior13();
        [[nodiscard]] bool Behavior14();
        [[nodiscard]] bool Behavior15();
        [[nodiscard]] bool Behavior16();
        [[nodiscard]] bool Behavior17();
        [[nodiscard]] bool Behavior18();
        [[nodiscard]] bool Behavior19();
        [[nodiscard]] bool Behavior20();
        [[nodiscard]] bool Behavior21();
        [[nodiscard]] bool Behavior22();
        [[nodiscard]] bool Behavior23();
        [[nodiscard]] bool Behavior24();
        [[nodiscard]] bool Behavior25();

    private:
        [[nodiscard]] bool AnimEnded() const;
        void SetNodeAnim(std::int32_t id, AnimFlags flags = AnimFlags::None);
        void SpawnHitZone();
        void PickMoveTarget(CollisionVolume volume);
        [[nodiscard]] bool HandleCollision(OpenTK::Mathematics::Vector3 testPos);
        void StartRecoil();

        void State00();
        void State01();
        void State02();
        void State03();
        void State04();
        void State05();
        void State06();
        void State07();
        void State08();
        void State09();
        void State10();
        void State11();
        void State12();
        void State13();
        void State14();
        void State15();
        void State16();
        void State17();
        void State18();
        void State19();
    };
}
