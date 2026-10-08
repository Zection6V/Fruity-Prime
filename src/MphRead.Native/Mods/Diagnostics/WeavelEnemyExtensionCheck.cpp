#include "WeavelAltFormCheck.hpp"

#include "../Combat/Extensions/HalfturretEnemyExtension.hpp"
#include "../../Entities/EnemyInstanceEntity.hpp"
#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/Players/HalfturretEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Console.hpp"

#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    using namespace Entities;
    using OpenTK::Mathematics::Vector3;
    using Combat::Extensions::HalfturretEnemyExtension;

    namespace
    {
        class TurretTargetEnemy final : public EnemyInstanceEntity
        {
        public:
            TurretTargetEnemy(Scene& scene, Vector3 center)
                : EnemyInstanceEntity({EnemyType::WarWasp, nullptr}, {}, &scene)
            {
                _health = _healthMax = 200;
                _hurtVolume = CollisionVolume(center, 0.75F);
                // An incorrect origin-based aim cannot hit this offset body.
                Position = center + Vector3(100, 100, 100);
                Flags = EnemyFlags::CollideBeam;
                BeamEffectiveness.fill(Effectiveness::Normal);
            }
            void Initialize() override {}
            void MoveBody(Vector3 center) { _hurtVolume = CollisionVolume(center, 0.75F); }
            int Hits = 0;
        protected:
            bool EnemyTakeDamage(EntityBase*) override { ++Hits; return false; }
        };
    }

    void WeavelAltFormCheck::CheckEnemyExtension(Scene& scene, PlayerEntity& owner,
        HalfturretEntity& turret)
    {
        int checks = 0;
        const auto check = [&checks](bool ok, const std::string& name)
        {
            if (!ok) throw std::runtime_error("Adventure extension: " + name);
            ++checks;
            NativeRuntime::ConsoleWriteLine("WEAVEL EXTENSION PASS " + name);
        };
        const Vector3 base(1000, 1000, 1000);
        for (const auto& player : PlayerEntity::Players()) player->SetHealth(0);
        owner.ModForceWeavelState(false, false);
        owner.SetHealth(100);
        owner.SetIsBot(false);
        owner.Position = base + Vector3(100, 0, 0);
        owner.ModForceWeavelState(true, true);
        turret.Position = base;
        turret._grounded = true;
        turret._target.reset();
        GameState::Mode(GameMode::SinglePlayer);
        auto near = std::make_shared<TurretTargetEnemy>(scene, base + Vector3(0, 0.4F, 6));
        auto far = std::make_shared<TurretTargetEnemy>(scene, base + Vector3(0, 0.4F, 10));
        scene.AddEntity(far); scene.AddEntity(near);
        const auto find = [&] { return HalfturretEnemyExtension::FindTarget(scene, owner, base); };
        check(find() == near, "nearest hurt body selected despite offset origin");
        check(OpenTK::Mathematics::Equal(HalfturretEnemyExtension::AimPosition(*near),
            near->HurtVolume().GetCenter()), "enemy aim uses hurt-volume center");
        near->SetHealth(0);
        check(find() == far, "dead enemy skipped");
        near->SetHealth(200);
        near->Flags |= EnemyFlags::Invincible;
        check(find() == far, "invincible enemy skipped");
        near->Flags = static_cast<EnemyFlags>(0);
        check(find() == far, "non-collidable enemy skipped");
        near->Flags = EnemyFlags::CollideBeam;
        near->BeamEffectiveness[static_cast<std::size_t>(BeamType::Battlehammer)] = Effectiveness::Zero;
        check(find() == far, "Battlehammer immune enemy skipped");
        near->BeamEffectiveness.fill(Effectiveness::Normal);
        near->MoveBody(base + Vector3(15, 0, 0));
        check(find() == far, "strict native fifteen-unit range preserved");
        near->MoveBody(base + Vector3(0, 0.4F, 6));
        owner.SetIsBot(true);
        check(find() == nullptr, "Story hunter opponent does not acquire Adventure enemies");
        owner.SetIsBot(false);
        GameState::Mode(GameMode::Battle);
        check(find() == nullptr, "multiplayer does not enable enemy extension");
        GameState::Mode(GameMode::SinglePlayer);

        const auto step = [&](std::uint64_t frame)
        {
            scene._frameCount = frame;
            check(turret.Process(), "production turret remains active");
        };
        owner.SetTimeSinceShot(255);
        step(2001);
        check(turret.Target() == nullptr && owner.TimeSinceShot() != 0,
            "odd sibling neither acquires nor fires");
        step(2002);
        check(turret.Target() == near && owner.TimeSinceShot() == 0,
            "native tick acquires enemy and fires using existing cooldown");
        std::vector<std::shared_ptr<BeamProjectileEntity>> beams;
        auto enumerator = scene.GetBeamProjectileEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            auto beam = enumerator.Current();
            if (beam->Owner().get() == &turret) beams.push_back(beam);
        }
        check(!beams.empty() && beams.back()->Beam() == BeamType::Battlehammer,
            "turret spawns its existing Battlehammer projectile");
        std::vector<bool> active(beams.size(), true);
        for (int frame = 0; frame < 180 && near->Hits == 0; ++frame)
        {
            scene._frameCount = 2003 + frame;
            for (std::size_t i = 0; i < beams.size(); ++i)
                if (active[i]) active[i] = beams[i]->Process();
        }
        check(near->Health() < 200 && near->Hits > 0,
            "actual projectile hits offset Adventure enemy and deals damage");

        turret.OnTakeDamage(near, 1);
        near->SetHealth(0);
        owner.SetTimeSinceShot(0);
        step(2200);
        check(turret.Target() == far, "dead retained enemy is replaced immediately");
        turret.OnTakeDamage(far, 1);
        scene.RemoveEntity(far);
        step(2202);
        check(turret.Target() == nullptr, "removed live enemy is released immediately");
        scene.AddEntity(far);
        turret.OnTakeDamage(far, 1);
        far->Flags |= EnemyFlags::Invincible;
        step(2204);
        check(turret.Target() == nullptr, "retained enemy becoming invincible is released");
        far->Flags = EnemyFlags::CollideBeam;
        turret.OnTakeDamage(far, 1);
        far->MoveBody(base + Vector3(16, 0, 0));
        step(2206);
        check(turret.Target() == nullptr, "retained enemy leaving range is released");
        far->MoveBody(base + Vector3(0, 0.4F, 10));
        turret._freezeTimer = 2;
        owner.SetTimeSinceShot(255);
        step(2208);
        check(turret.Target() == nullptr && owner.TimeSinceShot() != 0,
            "freeze blocks extension acquisition and firing");
        turret._freezeTimer = 0;

        auto hunter = PlayerEntity::Players()[1];
        hunter->SetHealth(100);
        hunter->Position = base + Vector3(0, 0, 12);
        hunter->_curAlpha = 1;
        owner.SetTimeSinceShot(0);
        step(2210);
        check(turret.Target() == hunter, "ROM hunter acquisition retains priority over nearer enemy");
        check(OpenTK::Mathematics::Equal(HalfturretEnemyExtension::AimPosition(*hunter), hunter->Position),
            "ROM hunter aim position unchanged");
        check(HalfturretEnemyExtension::KeepTarget(scene, owner, *hunter, base),
            "extension leaves native hunter retaliation target untouched");
        turret._target.reset();
        scene.RemoveEntity(near); scene.RemoveEntity(far);
        GameState::Mode(GameMode::Battle);
        NativeRuntime::ConsoleWriteLine("WEAVEL EXTENSION PASS " + std::to_string(checks) + " checks");
    }
}
