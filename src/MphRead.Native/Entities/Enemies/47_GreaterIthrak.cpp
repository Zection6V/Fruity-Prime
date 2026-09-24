#include "47_GreaterIthrak.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

using ::MphRead::NativeRuntime::RequireReference;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        [[nodiscard]] Enemy47Entity& RequireEnemy(Enemy47Entity* enemy)
        {
            return RequireReference(enemy);
        }
    }

    Enemy47Entity::Enemy47Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : Enemy46Entity(data, nodeRef, scene)
    {
    }

    void Enemy47Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        Setup(
            spawner.Data.Header.Position.ToFloatVector(),
            spawner.Data.Header.FacingVector.ToFloatVector(),
            0,
            spawner.Data.Fields.S05().Volume0,
            spawner.Data.Fields.S05().Volume1,
            spawner.Data.Fields.S05().Volume2,
            spawner.Data.Fields.S05().Volume3);
    }

    void Enemy47Entity::CallSubroutine()
    {
        (void)EnemyInstanceEntity::CallSubroutine<Enemy47Entity>(
            Metadata::Enemy47Subroutines, this);
    }

    void Enemy47Entity::UpdateMouthMaterial()
    {
        RequireReference(_mouthMaterial).Diffuse = ColorRgb(14, 14, 14);
    }

    bool Enemy47Entity::Behavior00(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior00();
    }

    bool Enemy47Entity::Behavior01(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior01();
    }

    bool Enemy47Entity::Behavior02(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior02();
    }

    bool Enemy47Entity::Behavior03(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior03();
    }

    bool Enemy47Entity::Behavior04(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior04();
    }

    bool Enemy47Entity::Behavior05(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior05();
    }

    bool Enemy47Entity::Behavior06(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior06();
    }

    bool Enemy47Entity::Behavior07(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior07();
    }

    bool Enemy47Entity::Behavior08(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior08();
    }

    bool Enemy47Entity::Behavior09(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior09();
    }

    bool Enemy47Entity::Behavior10(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior10();
    }

    bool Enemy47Entity::Behavior11(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior11();
    }

    bool Enemy47Entity::Behavior12(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior12();
    }

    bool Enemy47Entity::Behavior13(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior13();
    }

    bool Enemy47Entity::Behavior14(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior14();
    }

    bool Enemy47Entity::Behavior15(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior15();
    }

    bool Enemy47Entity::Behavior16(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior16();
    }

    bool Enemy47Entity::Behavior17(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior17();
    }

    bool Enemy47Entity::Behavior18(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior18();
    }

    bool Enemy47Entity::Behavior19(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior19();
    }

    bool Enemy47Entity::Behavior20(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior20();
    }

    bool Enemy47Entity::Behavior21(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior21();
    }

    bool Enemy47Entity::Behavior22(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior22();
    }

    bool Enemy47Entity::Behavior23(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior23();
    }

    bool Enemy47Entity::Behavior24(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior24();
    }

    bool Enemy47Entity::Behavior25(Enemy47Entity* enemy)
    {
        return RequireEnemy(enemy).Enemy46Entity::Behavior25();
    }
}
