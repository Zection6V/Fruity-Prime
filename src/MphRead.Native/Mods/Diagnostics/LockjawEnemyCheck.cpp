#include "LockjawEnemyCheck.hpp"

#include "../Headless.hpp"
#include "../Input/SyntheticInput.hpp"
#include "../Combat/LockjawCollision.hpp"
#include "../../Entities/BombEntity.hpp"
#include "../../Entities/EnemyInstanceEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"

#include <array>
#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    using namespace Entities;
    using OpenTK::Mathematics::Vector2i;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        class TargetEnemy final : public EnemyInstanceEntity
        {
        public:
            TargetEnemy(Scene* scene, CollisionVolume volume, EnemyFlags flags)
                : EnemyInstanceEntity({EnemyType::WarWasp, nullptr}, {}, scene)
            {
                _health = _healthMax = 200;
                _hurtVolume = volume;
                Flags = flags;
                // Deliberately disagree with the hurt volume: offset enemies
                // and tracked bombs must use the body rather than the origin.
                Position = volume.GetCenter() + Vector3(100, 100, 100);
            }
            void Initialize() override {}
            int Hits = 0;
        protected:
            bool EnemyTakeDamage(EntityBase*) override { ++Hits; return false; }
        };
    }

    std::int32_t LockjawEnemyCheck::Run(const std::string& room)
    {
        Mods::Headless::Enter();
        auto keyboard = Input::SyntheticInput::CreateKeyboard();
        auto mouse = Input::SyntheticInput::CreateMouse();
        auto scene = std::make_shared<Scene>(Vector2i(256, 192), *keyboard, *mouse,
            [](std::string) {}, [] {});
        int checks = 0;
        const auto check = [&checks](bool ok, const std::string& name)
        {
            if (!ok) throw std::runtime_error(name);
            ++checks;
            NativeRuntime::ConsoleWriteLine("LOCKJAW PASS " + name);
        };
        try
        {
            GameState::Mode(GameMode::SinglePlayer);
            PlayerEntity::SetMaxPlayers(1);
            auto owner = PlayerEntity::Create(Hunter::Sylux, 0);
            scene->AddRoom(room, GameMode::SinglePlayer, 1);
            scene->OnLoad();
            owner->SetHealth(100);
            owner->Position = Vector3(2000, 2000, 2000);
            owner->SetIsBot(false);
            const Vector3 base(1000, 1000, 1000);
            using Mods::Combat::LockjawCollision;
            const LockjawCollision::Triangle triangle{
                base, base + Vector3(6, 0, 0), base + Vector3(3, 0, 6)};
            check(LockjawCollision::SnareContainsPoint(triangle, base + Vector3(3, 0, 2)),
                "hunter snare contains interior point");
            check(LockjawCollision::SnareContainsPoint(
                {triangle[0], triangle[2], triangle[1]}, base + Vector3(3, 0, 2)),
                "hunter snare accepts reversed winding");
            check(!LockjawCollision::SnareContainsPoint(triangle, base + Vector3(3, 0, 0)),
                "hunter snare excludes its edge");
            check(!LockjawCollision::SnareContainsPoint(triangle, base + Vector3(7, 0, 2)),
                "hunter snare excludes exterior point");
            check(LockjawCollision::SnareContainsPoint(triangle, base + Vector3(3, 0.74F, 2)),
                "hunter snare includes point within plane tolerance");
            check(!LockjawCollision::SnareContainsPoint(triangle, base + Vector3(3, 0.75F, 2))
                && !LockjawCollision::SnareContainsPoint(triangle, base + Vector3(3, -0.75F, 2)),
                "hunter snare excludes both thickness boundaries");
            check(!LockjawCollision::SnareContainsPoint(
                {base, base + Vector3(3, 0, 0), base + Vector3(6, 0, 0)}, base + Vector3(3, 0, 2)),
                "hunter snare rejects collinear bombs");
            std::vector<std::shared_ptr<BombEntity>> bombs;
            std::shared_ptr<TargetEnemy> enemy;
            const auto reset = [&](std::initializer_list<Vector3> points, CollisionVolume volume,
                EnemyFlags flags = EnemyFlags::CollideBeam)
            {
                for (auto it = bombs.rbegin(); it != bombs.rend(); ++it)
                {
                    (*it)->Destroy();
                    scene->RemoveEntity(*it);
                }
                bombs.clear();
                if (enemy) { scene->RemoveEntity(enemy); enemy->Destroy(); }
                check(owner->SyluxBombCount() == 0, "pooled chain released");
                enemy = std::make_shared<TargetEnemy>(scene.get(), volume, flags);
                scene->AddEntity(enemy);
                for (const Vector3 point : points)
                {
                    auto bomb = BombEntity::Spawn(owner.get(), EntityBase::GetTransformMatrix(
                        Vector3::UnitZ, Vector3::UnitY, base + point), scene.get());
                    check(bomb != nullptr, "pooled bomb spawned");
                    bomb->SetBombIndex(owner->SyluxBombCount());
                    owner->SyluxBombs()[owner->SyluxBombCount()] = bomb;
                    owner->SetSyluxBombCount(owner->SyluxBombCount() + 1);
                    bomb->SetRadius(Fixed::ToFloat(owner->Values().BombRadius));
                    bomb->SetSelfRadius(Fixed::ToFloat(owner->Values().BombSelfRadius));
                    bomb->SetDamage(owner->Values().BombDamage);
                    bomb->SetEnemyDamage(owner->Values().BombEnemyDamage);
                    bombs.push_back(bomb);
                }
            };
            const auto runChain = [&]
            {
                std::array<bool, 3> active{true, true, true};
                for (int frame = 0; frame < 60; ++frame)
                    for (std::size_t i = 0; i < bombs.size(); ++i)
                        if (active[i]) active[i] = bombs[i]->Process();
            };
            const auto sphere = [&](Vector3 p, float r = 0.2F) { return CollisionVolume(base + p, r); };
            reset({Vector3::Zero}, sphere(Vector3(0.8F, 0, 0)));
            (void)bombs[0]->Process();
            check(enemy->Health() == 200 - owner->Values().BombEnemyDamage
                && TestFlag(bombs[0]->Flags(), BombFlags::Exploded), "single bomb hits offset body");

            reset({Vector3::Zero, Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 0)));
            (void)bombs[1]->Process();
            check(enemy->Health() == 180 && enemy->Hits == 1, "two-bomb wire deals 20 once");
            runChain();
            check(enemy->Health() == 180 - 2 * owner->Values().BombEnemyDamage,
                "both triggered bombs home to the enemy and explode");

            reset({Vector3::Zero, Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 0)));
            enemy->SetHealth(20);
            (void)bombs[1]->Process();
            check(enemy->Health() == 0, "wire kills an adventure enemy");
            std::weak_ptr<TargetEnemy> removedEnemy = enemy;
            enemy->Destroy();
            scene->RemoveEntity(enemy);
            enemy.reset();
            check(!removedEnemy.expired(), "homing chain retains removed enemy safely");
            // The owner also retains its most recent impact for the HUD.
            // Verify that each bomb releases its own reference independently.
            const auto retainedReferences = removedEnemy.use_count();
            for (const auto& bomb : bombs) (void)bomb->Process();
            check(removedEnemy.use_count() + static_cast<long>(bombs.size()) == retainedReferences,
                "dead target released by the whole chain");

            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)}, sphere(Vector3(4.5F, 0, 3)));
            (void)bombs[2]->Process();
            check(enemy->Health() == 180, "third-bomb wire to second bomb hits");
            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)}, sphere(Vector3(1.5F, 0, 3)));
            (void)bombs[2]->Process();
            check(enemy->Health() == 180, "third-bomb wire to first bomb hits");

            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)}, sphere(Vector3(3, 0, 2)));
            (void)bombs[0]->Process();
            check(bombs[0]->EnemyDamage() == 60 && bombs[1]->EnemyDamage() == 60
                && bombs[2]->EnemyDamage() == 60, "triangle arms all three bombs at 60");
            runChain();
            check(enemy->Health() == 20 && enemy->Hits == 3, "triangle deals three 60-damage impacts");

            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)},
                CollisionVolume(Vector3::UnitY, base + Vector3(3, 0.1F, 2), 0.3F, 3));
            (void)bombs[0]->Process();
            check(bombs[0]->EnemyDamage() == 60, "triangle catches a tall cylinder body");

            reset({Vector3::Zero, Vector3(6, 0, 0)},
                CollisionVolume(Vector3::UnitY, base + Vector3(3, -1, 0), 0.3F, 3));
            (void)bombs[1]->Process();
            check(enemy->Health() == 180, "wire hits cylinder body");
            reset({Vector3::Zero, Vector3(6, 0, 0)},
                CollisionVolume(Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ,
                    base + Vector3(2.5F, -0.5F, -0.5F), 1, 1, 1));
            (void)bombs[1]->Process();
            check(enemy->Health() == 180, "wire hits box body");
            reset({Vector3::Zero, Vector3(6, 0, 0)},
                CollisionVolume(Vector3::UnitZ, Vector3::UnitY, -Vector3::UnitX,
                    base + Vector3(3.5F, -0.5F, -0.5F), 1, 1, 1));
            (void)bombs[1]->Process();
            check(enemy->Health() == 180, "wire hits rotated box body");
            reset({Vector3::Zero, Vector3(6, 0, 0)},
                CollisionVolume(Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ,
                    base + Vector3(2.5F, -0.5F, 2), 1, 1, 1));
            (void)bombs[1]->Process();
            check(enemy->Health() == 200 && bombs[1]->Countdown() > 44, "wire misses separate box");

            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)}, sphere(Vector3(3, 0, 2)),
                EnemyFlags::NoBombDamage | EnemyFlags::CollideBeam);
            (void)bombs[0]->Process();
            runChain();
            check(enemy->Health() == 200, "triangle homing explosions respect bomb immunity");

            for (const auto flags : {EnemyFlags::Invincible | EnemyFlags::CollideBeam,
                EnemyFlags::NoBombDamage | EnemyFlags::CollideBeam, static_cast<EnemyFlags>(0)})
            {
                reset({Vector3::Zero, Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 0)), flags);
                (void)bombs[1]->Process();
                check(enemy->Health() == 200, "wire respects enemy protection flags");
            }
            reset({Vector3::Zero, Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 2)));
            (void)bombs[1]->Process();
            check(enemy->Health() == 200 && bombs[1]->Countdown() > 44, "wire miss does not trigger homing");
            reset({Vector3::Zero, Vector3(6, 0, 0), Vector3(3, 0, 6)}, sphere(Vector3(3, 3, 2)));
            (void)bombs[0]->Process();
            check(bombs[0]->EnemyDamage() == owner->Values().BombEnemyDamage, "enemy above triangle is not snared");
            reset({Vector3::Zero, Vector3(3, 0, 0), Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 2)));
            (void)bombs[0]->Process();
            check(enemy->Health() == 200 && bombs[0]->EnemyDamage() == owner->Values().BombEnemyDamage,
                "degenerate triangle does not damage an enemy");
            reset({Vector3::Zero, Vector3(6, 0, 0)}, sphere(Vector3(3, 0, 0)));
            enemy->SetHealth(0);
            (void)bombs[1]->Process();
            check(enemy->Hits == 0 && bombs[1]->Countdown() > 44, "dead enemy does not trigger wire");
            NativeRuntime::ConsoleWriteLine("LOCKJAW PASS " + std::to_string(checks) + " checks");
            scene->DoCleanup();
            return 0;
        }
        catch (...)
        {
            NativeRuntime::ConsoleWriteLine("LOCKJAW FAIL " + NativeRuntime::ExceptionToString(std::current_exception()));
            scene->DoCleanup();
            return 1;
        }
    }
}
