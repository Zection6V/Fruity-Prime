#pragma once

#include "46_LesserIthrak.hpp"

namespace MphRead::Entities::Enemies
{
    class Enemy47Entity : public Enemy46Entity
    {
    public:
        Enemy47Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy47Entity(const Enemy47Entity&) = delete;
        Enemy47Entity& operator=(const Enemy47Entity&) = delete;
        Enemy47Entity(Enemy47Entity&&) = delete;
        Enemy47Entity& operator=(Enemy47Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior13(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior14(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior15(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior16(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior17(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior18(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior19(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior20(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior21(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior22(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior23(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior24(Enemy47Entity* enemy);
        [[nodiscard]] static bool Behavior25(Enemy47Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void CallSubroutine() override;
        void UpdateMouthMaterial() override;
    };
}
