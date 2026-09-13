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
    class Enemy38Entity : public EnemyInstanceEntity
    {
    public:
        Enemy38Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy38Entity(const Enemy38Entity&) = delete;
        Enemy38Entity& operator=(const Enemy38Entity&) = delete;
        Enemy38Entity(Enemy38Entity&&) = delete;
        Enemy38Entity& operator=(Enemy38Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior13(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior14(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior15(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior16(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior17(Enemy38Entity* enemy);
        [[nodiscard]] static bool Behavior18(Enemy38Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        CollisionVolume _volume1{};
        CollisionVolume _volume2{};
        OpenTK::Mathematics::Vector3 _initalPos = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _initalFacing = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _targetVec = OpenTK::Mathematics::Vector3::Zero;

        std::uint16_t _jumpTimer = 0;
        std::uint16_t _delayTimer = 0;
        float _jumpHeight = 0.0F;
        float _aimAngleStep = 0.0F;
        std::uint16_t _aimSteps = 0;

        void State00();
        void State01();
        void State02();
        void State03();
        void SetCameraShake(float shakeMax);
        void DoThing(float shakeMax);
        void State04();
        void State05();
        void State06();
        void State07();
        void State08();
        void State09();
        void State10();
        void State11();
        void FaceInitialPosition();
        void State12();
        void State13();
        void State14();
        void State15();
        void State16();

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
    };
}
