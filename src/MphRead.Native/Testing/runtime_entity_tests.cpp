#include "Entities/runtime_entities.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        using namespace fruityprime::runtime;

        BeamProjectileEntity projectile;
        require(BeamProjectileEntity::Charged == 0x2
                    && BeamProjectileEntity::Continuous == 0x40
                    && BeamProjectileEntity::DestroyMuzzle == 0x2000,
                "BeamFlags values differ from BeamProjectileEntity.cs");
        projectile.position = {1.0F, 2.0F, 3.0F};
        projectile.direction = {0.0F, 0.0F, 1.0F};
        projectile.speed = 10.0F;
        projectile.max_lifetime = 0.2F;
        require(projectile.step(0.1F), "projectile expired too early");
        require(std::abs(projectile.position.z - 4.0F) < 0.001F,
                "projectile movement mismatch");
        projectile.spawn_position = {1.0F, 2.0F, 3.0F};
        projectile.back_position = {1.0F, 2.0F, 3.0F};
        projectile.past_positions.fill({1.0F, 2.0F, 3.0F});
        projectile.reposition({2.0F, -1.0F, 0.5F});
        require(std::abs(projectile.spawn_position.x - 3.0F) < 0.001F
                    && std::abs(projectile.back_position.y - 1.0F) < 0.001F
                    && std::abs(projectile.past_positions[9].z - 3.5F)
                        < 0.001F,
                "projectile trail state was not repositioned with the beam");
        require(!projectile.step(0.2F), "projectile lifetime was ignored");

        const FhItemInstanceEntityData fh_item_data{
            {1.0F, 2.0F, 3.0F}, fruityprime::formats::FhItemType::Missile};
        FhItemEntity fh_item1(100, fh_item_data);
        FhItemEntity fh_item2(101, fh_item_data);
        require(fh_item1.position().y == 2.5F
                    && fh_item1.item_type()
                        == fruityprime::formats::FhItemType::Missile
                    && std::abs(fh_item2.spin() - fh_item1.spin() - 45.0F)
                        < 0.001F,
                "FhItemEntity/SpinningEntityBase construction mismatch");
        const float fh_spin = fh_item1.spin();
        require(fh_item1.process(1.0F / 60.0F)
                    && std::abs(fh_item1.spin() - fh_spin - 2.1F) < 0.001F,
                "SpinningEntityBase did not apply managed spin speed");

        ItemInstanceEntity item(1, 7, {0.0F, 0.0F, 0.0F});
        item.set_despawn_seconds(0.1F);
        require(item.process(0.05F), "item despawned too early");
        require(!item.process(0.06F), "item despawn timer was ignored");

        BombEntity bomb(2, BombType::MorphBall, 0, {}, {1.0F, 0.0F, 0.0F});
        bomb.arm(0.1F);
        require(bomb.process(0.1F) == false && bomb.exploded(),
                "bomb did not trigger at the armed time");

        EnemyInstanceEntity enemy(3, 4, {});
        require(enemy.scan_id() == 224, "enemy metadata scan ID mismatch");
        require(enemy.effectiveness()[0] ==
                    fruityprime::metadata::Effectiveness::Normal,
                "enemy metadata effectiveness mismatch");
        enemy.set_health(5);
        require(enemy.take_damage(3) && enemy.health() == 2,
                "enemy damage mismatch");
        require(enemy.take_damage(2) && !enemy.active(),
                "enemy death was not applied");

        const std::array<fruityprime::net::Vec3, 1> close_players{{
            {0.0F, 0.0F, 0.0F}}};
        EnemySpawnerEntity spawner(
            5, 1, {}, 2, 2, 2, 1, 1, true, false, 4.0F);
        spawner.set_has_model(true);
        auto spawn_result = spawner.tick(1.0F / 60.0F, close_players);
        require(spawn_result.spawned == 0 && spawner.suspended(),
                "enemy spawner ignored its initial cooldown");
        spawn_result = spawner.tick(1.0F / 60.0F, close_players);
        require(spawn_result.spawned == 2
                    && spawner.active_count() == 2
                    && spawner.spawned_count() == 2
                    && (spawner.flags() & EnemySpawnerEntity::PlayAnimation) != 0,
                "enemy spawner did not honor its batch and limit");
        require(!spawner.on_enemy_destroyed(),
                "enemy spawner completed before its last enemy was destroyed");
        static_cast<void>(spawner.tick(1.0F / 60.0F, close_players));
        static_cast<void>(spawner.tick(1.0F / 60.0F, close_players));
        require(spawner.on_enemy_destroyed(),
                "enemy spawner did not deactivate after its total was exhausted");
        require(!spawner.spawner_active(),
                "enemy spawner remained active after completion");

        EnemySpawnerEntity retry_spawner(
            6, 1, {}, 2, 1, 1, 0, 0, true, false, 3.0F);
        require(retry_spawner.tick(1.0F / 60.0F, close_players).spawned == 1,
                "enemy spawner did not create its first retry target");
        require(!retry_spawner.on_enemy_destroyed(true)
                    && retry_spawner.spawned_count() == 0,
                "out-of-range enemy destruction did not free its total slot");
        require(retry_spawner.tick(1.0F / 60.0F, close_players).spawned == 1,
                "enemy spawner did not retry an out-of-range enemy");

        EntityPool message_pool;
        auto& pooled_item = message_pool.emplace<ItemInstanceEntity>(
            7, 3, fruityprime::net::Vec3{});
        auto& pooled_enemy = message_pool.emplace<EnemyInstanceEntity>(
            8, 1, fruityprime::net::Vec3{});
        pooled_enemy.set_health(5);
        auto& pooled_bomb = message_pool.emplace<BombEntity>(
            9, BombType::MorphBall, 0, fruityprime::net::Vec3{},
            fruityprime::net::Vec3{});
        fruityprime::messaging::MessageInfo set_active;
        set_active.target = 7;
        set_active.cartridge_message = 5; // SetActive
        set_active.parameter1 = 0;
        require(message_pool.dispatch(set_active) == 1
                    && !pooled_item.active(),
                "runtime SetActive message was not dispatched");
        fruityprime::messaging::MessageInfo damage;
        damage.target = 8;
        damage.cartridge_message = 7; // Damage
        damage.parameter1 = 3;
        require(message_pool.dispatch(damage) == 1
                    && pooled_enemy.health() == 2,
                "runtime Damage message was not dispatched");
        fruityprime::messaging::MessageInfo impact;
        impact.target = 9;
        impact.cartridge_message = 20; // Impact
        require(message_pool.dispatch(impact) == 1
                    && pooled_bomb.exploded(),
                "runtime Impact message was not dispatched");

        BeamEffectEntity ice_wave(10, 0, 0.5F, {});
        require(ice_wave.uses_model()
                    && ice_wave.model_name() == "iceWave"
                    && ice_wave.spawned_effect_id() == 78,
                "ice-wave beam effect mapping mismatch");
        BeamEffectEntity sniper_beam(11, 1, 0.5F, {});
        require(sniper_beam.uses_model()
                    && sniper_beam.model_name() == "sniperBeam"
                    && sniper_beam.spawned_effect_id() == -1,
                "sniper beam effect mapping mismatch");
        BeamEffectEntity boss_burn(12, 2, 0.5F, {});
        require(boss_burn.uses_model()
                    && boss_burn.model_name() == "cylBossLaserBurn"
                    && boss_burn.spawned_effect_id() == -1,
                "boss laser beam effect mapping mismatch");
        BeamEffectEntity power_splat(13, 4, 0.5F, {});
        require(!power_splat.uses_model()
                    && power_splat.spawned_effect_id() == 1,
                "power beam effect mapping mismatch");
        power_splat.set_no_splat(true);
        require(power_splat.spawned_effect_id() == 2,
                "power beam NoSplat mapping mismatch");
        BeamEffectEntity charged_splat(14, 95, 0.5F, {});
        charged_splat.set_no_splat(true);
        require(charged_splat.spawned_effect_id() == 98,
                "charged beam NoSplat mapping mismatch");
        BeamEffectEntity invalid_beam(15, -1, 0.5F, {});
        require(!invalid_beam.uses_model()
                    && invalid_beam.spawned_effect_id() == -1,
                "invalid beam effect mapping mismatch");

        EntityPool pool;
        auto& effect = pool.emplace<BeamEffectEntity>(
            4, 1, 0.05F, fruityprime::net::Vec3{});
        static_cast<void>(effect);
        require(pool.process(0.1F) == 1 && pool.size() == 0,
                "entity pool did not remove expired entities");

        require(kind_name(Kind::Halfturret) == "halfturret",
                "runtime kind name mismatch");
        require(kind_name(Kind::EnemySpawner) == "enemy_spawner",
                "enemy spawner kind name mismatch");
        std::cout << "native runtime entity tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native runtime entity tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
