#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/39_FireSpawn.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <cmath>
#include <limits>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "Metadata/enemy_values.hpp"
#include "enemy_scene.hpp"
#include "gorea_common.hpp"
#include "39_FireSpawn.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

constexpr float Pi39 = 3.14159265358979323846F;

// The four effects, by the numbers the cartridge gives them.  A Fire
// Spawn and an Ice Spawn are the same class with different art, so every
// one of them comes in a pair.
constexpr std::uint16_t LavaDemonDive = 93u;
constexpr std::uint16_t IceDemonDive = 133u;
constexpr std::uint16_t LavaDemonRise = 95u;
constexpr std::uint16_t IceDemonRise = 132u;
constexpr std::uint16_t LavaDemonHurl = 94u;
constexpr std::uint16_t IceDemonHurl = 96u;

// Enemy39Entity's four animations.
constexpr std::uint8_t AnimThrowRight = 0u;
constexpr std::uint8_t AnimThrowLeft = 1u;
constexpr std::uint8_t AnimDive = 2u;
constexpr std::uint8_t AnimRise = 3u;

// Native counterpart of Enemy39Entity.
//
// The Fire Spawn -- and the Ice Spawn, which is the same class with
// different art.  It lives under the lava and only comes up when a player
// is inside the volume it watches, surfacing at a point it picks inside a
// second volume, so it never comes up in the same place twice running.
//
// The thing that makes it a boss rather than a turret is that it is only
// tangible for part of its cycle, and the window is decided by its
// animations rather than by a clock.  It rises; when the rise finishes it
// becomes solid and shootable; it throws a rolled number of fireballs,
// alternating hands; and when it is out of them it dives, going
// invulnerable again as it goes.  Shooting it is a question of being
// ready before it surfaces.
class Enemy39Entity final {
public:
    Enemy39Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy39Entity.EnemyInitialize.
    void EnemyInitialize();

    // Enemy39Entity.EnemyProcess.
    void EnemyProcess();

    // Enemy39Entity.EnemyTakeDamage.  Returns whether the hit is ignored;
    // a Fire Spawn's own health is handled by the caller, and this is
    // only what dying does besides.
    [[nodiscard]] bool EnemyTakeDamage();

    void set_frame_count(std::uint64_t frames) noexcept {
        frame_count_ = frames;
    }

private:
    void State0();
    void State1();
    void State2();
    void State3();
    void State4();
    void State5();

    [[nodiscard]] bool Behavior0();
    [[nodiscard]] bool Behavior1();
    [[nodiscard]] bool Behavior2();
    [[nodiscard]] bool Behavior3();
    [[nodiscard]] bool Behavior4();
    [[nodiscard]] bool Behavior5();
    [[nodiscard]] bool Behavior6();

    // Enemy39Entity.ChooseSurfaceLocation, CreateEffect and StartSubmerge.
    void ChooseSurfaceLocation();
    void CreateEffect();
    [[nodiscard]] bool StartSubmerge();

    [[nodiscard]] bool CallSubroutine();
    void CallStateProcess();

    void ContactDamagePlayer(std::uint32_t damage, bool knockback) const;
    void SetHitZone(bool collidable) const;
    void SetAnimation(std::uint8_t animation, bool paused, bool no_loop);
    void AdvanceAnimation();
    [[nodiscard]] bool AnimationEnded() const noexcept {
        return agent_.firespawn_animation_ended;
    }
    [[nodiscard]] bool IsIce() const noexcept {
        return agent_.firespawn_subtype == 1;
    }
    [[nodiscard]] const metadata::Enemy39Values& values() const noexcept {
        const std::size_t index = std::min<std::size_t>(
            agent_.firespawn_subtype, metadata::Enemy39ValuesTable.size() - 1);
        return metadata::Enemy39ValuesTable[index];
    }
    // Where the fireball leaves from.  The managed class reads the two
    // wrist nodes off the model; there is no model here, so it is the
    // body offset half a unit to whichever side the throwing hand is.
    [[nodiscard]] net::Vec3 WristPosition() const noexcept;

    const EnemyScene& scene_;
    EnemyState& agent_;
    net::PlayerState* main_;
    std::uint64_t frame_count_ = 0;
};

void Enemy39Entity::SetAnimation(const std::uint8_t animation,
                                 const bool paused, const bool no_loop) {
    agent_.firespawn_animation = animation;
    agent_.firespawn_animation_frame = 0;
    agent_.firespawn_animation_length = gorea::authored_animation_length(
        agent_.enemy_type, animation, 1);
    agent_.firespawn_animation_ended = false;
    agent_.firespawn_animation_paused = paused;
    static_cast<void>(no_loop);
}

// The cursor runs at half this head's rate, which is what the cartridge's
// ModelInstance does.
void Enemy39Entity::AdvanceAnimation() {
    if (agent_.firespawn_animation_paused
        || agent_.firespawn_animation_length == 0
        || frame_count_ == 0 || frame_count_ % 2 != 0) {  // todo: FPS stuff
        return;
    }
    if (agent_.firespawn_animation_frame + 1
            >= agent_.firespawn_animation_length) {
        agent_.firespawn_animation_ended = true;
        return;
    }
    ++agent_.firespawn_animation_frame;
}

net::Vec3 Enemy39Entity::WristPosition() const noexcept {
    const net::Vec3 right{agent_.facing.z, 0.0F, -agent_.facing.x};
    const float side = agent_.firespawn_wrist_id == 0 ? -0.75F : 0.75F;
    return {agent_.position.x + right.x * side,
            agent_.position.y + 0.5F,
            agent_.position.z + right.z * side};
}

void Enemy39Entity::EnemyInitialize() {
    agent_.visible = true;
    agent_.invulnerable = true;
    agent_.body_radius = 1.0F;
    agent_.firespawn_subtype = static_cast<std::uint8_t>(
        agent_.firespawn.subtype);
    const metadata::Enemy39Values& v = values();
    agent_.health = agent_.health_max = v.HealthMax;
    agent_.scan_id = static_cast<std::uint16_t>(v.ScanId);
    // Enemy39Entity._recolors: an Ice Spawn wears the second palette.
    static constexpr std::array<std::uint8_t, 11> Recolors{
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    const std::size_t version = std::min<std::size_t>(
        agent_.firespawn.version, Recolors.size() - 1);
    agent_.recolor = Recolors[version];
    agent_.firespawn_attack_timer =
        static_cast<std::uint32_t>(v.AttackDelay) * 2u;  // todo: FPS stuff
    const std::uint32_t span = v.AttackCountMax + 1u > v.AttackCountMin
        ? static_cast<std::uint32_t>(v.AttackCountMax + 1 - v.AttackCountMin)
        : 1u;
    agent_.firespawn_attacks_remaining = static_cast<std::uint16_t>(
        v.AttackCountMin + utility::get_random_int2(span));
    // Under the surface, paused on the rise's first frame, and state
    // nought is where it waits for somebody to walk in.
    SetAnimation(AnimRise, true, false);
    agent_.firespawn_anim_frame_count = agent_.firespawn_animation_length;
    agent_.firespawn_wrist_id = 1;
    agent_.firespawn_tangibility_timer = 0;
    agent_.firespawn_submerged = true;
    agent_.state = agent_.next_state = agent_.sub_id = 0;
}

void Enemy39Entity::EnemyProcess() {
    ContactDamagePlayer(values().ContactDamage, true);
    AdvanceAnimation();
    CallStateProcess();
}

bool Enemy39Entity::EnemyTakeDamage() {
    if (agent_.health == 0) {
        // The fireball in its hand goes with it, and so does the hit zone
        // that made its body shootable at all.
        agent_.firespawn_effect_id = 0;
        SetHitZone(false);
    }
    return false;
}

// Every state but one turns to face the player first.  The managed class
// notes that the Y should be zeroed before normalising and is not, which
// squashes the transform -- kept, because a Fire Spawn really does lean
// when you stand above it.
void Enemy39Entity::State0() {
    if (main_ != nullptr) {
        net::Vec3 facing{main_->position.x - agent_.position.x,
                         main_->position.y - agent_.position.y,
                         main_->position.z - agent_.position.z};
        const float length = std::sqrt(facing.x * facing.x
                                       + facing.y * facing.y
                                       + facing.z * facing.z);
        if (length > 0.0F) {
            agent_.facing = {facing.x / length, 0.0F, facing.z / length};
            agent_.up = {0.0F, 1.0F, 0.0F};
        }
    }
    static_cast<void>(CallSubroutine());
}

void Enemy39Entity::State1() {
    static_cast<void>(CallSubroutine());
}

// Surfacing.  Ten frames after the rise begins it becomes solid -- both
// its own body and the hit zone that is what a player's shots actually
// land on.
void Enemy39Entity::State2() {
    if (agent_.firespawn_tangibility_timer == 5u * 2u) {  // todo: FPS stuff
        SetHitZone(true);
        agent_.invulnerable = false;
    }
    if (agent_.firespawn_tangibility_timer <= 5u * 2u) {
        ++agent_.firespawn_tangibility_timer;
    }
    State0();
}

void Enemy39Entity::State3() {
    State0();
}

// The throw.  The frame numbers are counted down from the animation's own
// length, so they are positions in the throw rather than elapsed time:
// fifty-three is when the fire appears in its hand, twenty-five is when
// it lets go, and in between the fireball follows the wrist.
void Enemy39Entity::State4() {
    State0();
    if (agent_.firespawn_attacks_remaining == 0
        || agent_.firespawn_anim_frame_count <= 0
        || frame_count_ == 0 || frame_count_ % 2 != 0) {  // todo: FPS stuff
        return;
    }
    if (AnimationEnded()) {
        // The hands alternate, and each throw restarts the countdown.
        if (agent_.firespawn_animation == AnimThrowLeft) {
            SetAnimation(AnimThrowRight, false, true);
            agent_.firespawn_wrist_id = 0;
        } else if (agent_.firespawn_animation == AnimThrowRight) {
            SetAnimation(AnimThrowLeft, false, true);
            agent_.firespawn_wrist_id = 1;
        }
        agent_.firespawn_anim_frame_count =
            agent_.firespawn_animation_length;
    }
    if (agent_.firespawn_anim_frame_count == 53) {
        CreateEffect();
    } else if (agent_.firespawn_anim_frame_count == 25) {
        --agent_.firespawn_attacks_remaining;
        agent_.firespawn_attack_timer =
            static_cast<std::uint32_t>(values().AttackDelay) * 2u;
        if (main_ != nullptr && scene_.SpawnProjectile) {
            const net::Vec3 from = WristPosition();
            net::Vec3 direction{main_->position.x - from.x,
                                main_->position.y + 0.5F - from.y,
                                main_->position.z - from.z};
            direction = normalized_or(direction, agent_.facing);
            scene_.SpawnProjectile(agent_, from, direction);
        }
        // Let go: the fireball is now the projectile's, not the hand's.
        agent_.firespawn_effect_id = 0;
    } else if (agent_.firespawn_anim_frame_count >= 26
               && agent_.firespawn_anim_frame_count <= 52
               && agent_.firespawn_effect_id != 0 && scene_.MoveEffect) {
        scene_.MoveEffect(agent_.firespawn_effect_id, WristPosition(),
                          agent_.facing);
    }
    --agent_.firespawn_anim_frame_count;
}

void Enemy39Entity::CreateEffect() {
    if (!scene_.SpawnPersistentEffect) {
        return;
    }
    agent_.firespawn_effect_id = scene_.SpawnPersistentEffect(
        IsIce() ? IceDemonHurl : LavaDemonHurl, WristPosition(),
        agent_.facing, agent_.id);
}

// Diving.  Thirty-six frames in it stops being solid again, which is a
// good deal longer than the ten it took to become solid -- so the window
// closes slowly and there is time for a last shot.
void Enemy39Entity::State5() {
    if (agent_.firespawn_tangibility_timer == 18u * 2u) {  // todo: FPS stuff
        SetHitZone(false);
        agent_.invulnerable = true;
    }
    if (agent_.firespawn_tangibility_timer <= 18u * 2u) {
        ++agent_.firespawn_tangibility_timer;
    }
    State0();
}

// Somebody walked into the volume it watches.  It picks where to come up
// and rolls how long to wait first.
bool Enemy39Entity::Behavior0() {
    if (main_ == nullptr
        || !agent_.firespawn.active_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    ChooseSurfaceLocation();
    const metadata::Enemy39Values& v = values();
    const std::uint32_t span = v.DiveTimerMax + 1u > v.DiveTimerMin
        ? static_cast<std::uint32_t>(v.DiveTimerMax + 1 - v.DiveTimerMin)
        : 1u;
    agent_.firespawn_dive_timer =
        (static_cast<std::uint32_t>(v.DiveTimerMin)
         + utility::get_random_int2(span)) * 2u;  // todo: FPS stuff
    return true;
}

// A point inside the second volume, at the height it already sits at.
// The angle's sign alternates, so a Fire Spawn works back and forth
// across its pool rather than circling it.
void Enemy39Entity::ChooseSurfaceLocation() {
    const auto& volume = agent_.firespawn.location_volume;
    float distance = 0.0F;
    if (volume.kind == scene::VolumeKind::Cylinder) {
        const auto radius = static_cast<std::uint32_t>(std::max(
            1.0F, std::round(volume.cylinder_radius * 4096.0F)));
        distance = static_cast<float>(utility::get_random_int2(radius))
            / 4096.0F;
    } else if (volume.kind == scene::VolumeKind::Sphere) {
        const auto radius = static_cast<std::uint32_t>(std::max(
            1.0F, std::round(volume.sphere_radius * 4096.0F)));
        distance = static_cast<float>(utility::get_random_int2(radius))
            / 4096.0F;
    } else {
        return;
    }
    agent_.firespawn_surface_direction =
        static_cast<std::int8_t>(-agent_.firespawn_surface_direction);
    const float angle =
        static_cast<float>(utility::get_random_int2(0xB4000u)) / 4096.0F
        * static_cast<float>(agent_.firespawn_surface_direction)
        * Pi39 / 180.0F;
    // A rotation about Y of (distance, 0, 0).
    const float x = distance * std::cos(angle);
    const float z = -distance * std::sin(angle);
    const auto centre = volume.kind == scene::VolumeKind::Cylinder
        ? volume.cylinder_position : volume.sphere_position;
    agent_.position = {centre.x + x, agent_.position.y, centre.z + z};
}

// Waiting to throw.
bool Enemy39Entity::Behavior1() {
    if (agent_.firespawn_attack_timer > 0) {
        --agent_.firespawn_attack_timer;
        return false;
    }
    SetAnimation(AnimThrowLeft, false, true);
    agent_.firespawn_attack_timer =
        static_cast<std::uint32_t>(values().AttackDelay) * 2u;
    return true;
}

// The dive finished: back under, and paused there.
bool Enemy39Entity::Behavior2() {
    if (!AnimationEnded()) {
        return false;
    }
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(IsIce() ? IceDemonDive : LavaDemonDive,
                           agent_.position, agent_.id);
    }
    SetAnimation(AnimRise, true, false);
    agent_.firespawn_tangibility_timer = 0;
    agent_.firespawn_submerged = true;
    return true;
}

// The rise finished: solid, and shootable.
bool Enemy39Entity::Behavior3() {
    if (!AnimationEnded()) {
        return false;
    }
    agent_.invulnerable = false;
    agent_.firespawn_tangibility_timer = 0;
    agent_.firespawn_submerged = false;
    return true;
}

// The wait before surfacing.
bool Enemy39Entity::Behavior4() {
    if (agent_.firespawn_dive_timer > 0) {
        --agent_.firespawn_dive_timer;
        return false;
    }
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(IsIce() ? IceDemonRise : LavaDemonRise,
                           agent_.position, agent_.id);
    }
    SetAnimation(AnimRise, false, true);
    return true;
}

// Out of fireballs.
bool Enemy39Entity::Behavior5() {
    if (agent_.firespawn_attacks_remaining > 0) {
        return false;
    }
    return StartSubmerge();
}

// The player left the volume.  Either way it goes down, and either way it
// waits for the current throw to finish first -- which is why a Fire
// Spawn always gets its last fireball off.
bool Enemy39Entity::Behavior6() {
    if (main_ != nullptr
        && agent_.firespawn.active_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    return StartSubmerge();
}

bool Enemy39Entity::StartSubmerge() {
    if (!AnimationEnded()) {
        return false;
    }
    SetAnimation(AnimDive, false, true);
    agent_.firespawn_anim_frame_count = agent_.firespawn_animation_length;
    const metadata::Enemy39Values& v = values();
    const std::uint32_t span = v.AttackCountMax + 1u > v.AttackCountMin
        ? static_cast<std::uint32_t>(v.AttackCountMax + 1 - v.AttackCountMin)
        : 1u;
    agent_.firespawn_attacks_remaining = static_cast<std::uint16_t>(
        v.AttackCountMin + utility::get_random_int2(span));
    agent_.firespawn_wrist_id = 1;
    agent_.firespawn_tangibility_timer = 0;
    agent_.invulnerable = true;
    return true;
}

void Enemy39Entity::ContactDamagePlayer(const std::uint32_t damage,
                                        const bool knockback) const {
    if (main_ == nullptr || agent_.invulnerable || !scene_.ContactDamage) {
        return;
    }
    const float reach = agent_.body_radius + 0.45F;
    if (distance_squared(agent_.position, main_->position) > reach * reach) {
        return;
    }
    if (knockback) {
        const float dx = main_->position.x - agent_.position.x;
        const float dy = main_->position.y - agent_.position.y;
        const float dz = main_->position.z - agent_.position.z;
        const float mag = std::sqrt(dx * dx + dy * dy + dz * dz) * 5.0F;
        if (mag > 0.0F) {
            main_->speed.x += dx / mag;
            main_->speed.z += dz / mag;
        }
    }
    scene_.ContactDamage(agent_, main_->slot_index, damage);
}

// Enemy50Entity, the linked hit zone.  A Fire Spawn's own body is not
// what a shot lands on -- the cylinder it stands in is -- so making it
// shootable means making that collidable.
void Enemy39Entity::SetHitZone(const bool collidable) const {
    if (scene_.SetHitZone) {
        scene_.SetHitZone(agent_, collidable);
    }
}

bool Enemy39Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy39Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior0();
            case 1: return Behavior1();
            case 2: return Behavior2();
            case 3: return Behavior3();
            case 4: return Behavior4();
            case 5: return Behavior5();
            case 6: return Behavior6();
            default: return false;
            }
        });
}

void Enemy39Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    case 4: State4(); break;
    case 5: State5(); break;
    default: break;
    }
}

} // namespace

void Session::update_firespawn(EnemyState& agent) {
    if (!agent.firespawn.supported) {
        return;
    }
    net::PlayerState* main = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest) {
            nearest = distance;
            main = &player;
        }
    }
    agent.target_slot = main != nullptr && !agent.firespawn_submerged
        ? main->slot_index : 0xff;

    // EnemyInstanceEntity.BaseProcess.  A Fire Spawn does not move: it
    // teleports between surfacings, so there is no velocity step.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;

    EnemyScene scene = build_enemy_scene(agent.position);
    scene.SpawnProjectile = [this](const EnemyState& shooter,
                                   net::Vec3 position, net::Vec3 direction) {
        spawn_enemy_projectile(shooter, position, direction);
    };
    scene.SpawnEffect = [this](std::uint32_t effect, net::Vec3 position,
                               std::uint32_t owner) {
        static_cast<void>(spawn_effect(
            static_cast<std::uint16_t>(effect), position, {1.0F, 0.0F, 0.0F},
            owner));
    };
    scene.SpawnPersistentEffect = [this](std::uint32_t effect,
                                         net::Vec3 position,
                                         net::Vec3 direction,
                                         std::uint32_t owner) {
        return spawn_effect(static_cast<std::uint16_t>(effect), position,
                            direction, owner, 0.25F, false, true, true);
    };
    scene.MoveEffect = [this](std::uint32_t effect, net::Vec3 position,
                              net::Vec3 direction) {
        update_effect_transform(effect, position, direction);
    };
    scene.SetHitZone = [this](const EnemyState& owner, bool collidable) {
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != owner.id
                || child.enemy_type != static_cast<std::uint8_t>(
                    formats::EnemyType::HitZone)) {
                continue;
            }
            child.hit_zone_collidable = collidable;
            child.visible = collidable;
            child.invulnerable = true;
        }
    };
    Enemy39Entity entity(scene, agent, main);
    entity.set_frame_count(tick_count_);
    entity.EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_39_fire_spawn::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_39_fire_spawn {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy39Entity(empty, agent, nullptr).EnemyInitialize();
}

} // namespace fruityprime::enemy::module_39_fire_spawn

namespace fruityprime::enemy {

FireSpawnProfile decode_firespawn_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    FireSpawnProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::FireSpawn)
        || fields.size() < 8 + 4 * 64) {
        return result;
    }

    result.subtype = std::min<std::uint32_t>(detail::read_u32(fields, 0), 1u);
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4), 10u);
    result.location_volume = detail::read_volume(fields, 72, origin);
    result.active_volume = detail::read_volume(fields, 136, origin);

    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::uint32_t effectiveness;
        std::uint16_t attack_delay_frames;
        std::uint16_t attack_count_min;
        std::uint16_t attack_count_max;
        std::uint16_t dive_timer_min;
        std::uint16_t dive_timer_max;
    };
    static constexpr std::array<Values, 2> values{{
        {600, 30, 15, 12, 0x8955, 0, 3, 6, 1, 40},
        {600, 30, 0, 12, 0xB155, 0, 2, 5, 1, 50}
    }};
    const Values& selected = values[result.subtype];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.effectiveness = selected.effectiveness;
    result.attack_delay_frames = selected.attack_delay_frames;
    result.attack_count_min = selected.attack_count_min;
    result.attack_count_max = selected.attack_count_max;
    result.dive_timer_min = selected.dive_timer_min;
    result.dive_timer_max = selected.dive_timer_max;

    result.projectile_weapon = metadata::native_weapon_slot_from_beam(
        static_cast<std::int32_t>(result.version));
    struct ProjectileValues {
        std::uint8_t draw_func;
        std::uint16_t color;
        std::uint8_t collision_effect;
        std::uint8_t muzzle_effect;
        float speed;
        float lifetime;
    };
    static constexpr std::array<ProjectileValues, 3> projectile_values{{
        {21, 9055, 242, 65, 2662.0F / 4096.0F * 60.0F,
         255.0F / 60.0F},
        {2, 32767, 89, 60, 3276.0F / 4096.0F * 60.0F,
         90.0F / 60.0F},
        {7, 32140, 8, 65, 819.0F / 4096.0F * 60.0F,
         255.0F / 60.0F}
    }};
    if (result.version < projectile_values.size()) {
        const auto& projectile = projectile_values[result.version];
        result.projectile_draw_func = projectile.draw_func;
        result.projectile_color = projectile.color;
        result.projectile_collision_effect = projectile.collision_effect;
        result.projectile_muzzle_effect = projectile.muzzle_effect;
        result.projectile_speed = projectile.speed;
        result.projectile_lifetime = projectile.lifetime;
    } else {
        if (result.projectile_weapon == 0xff) {
            result.projectile_weapon = 0;
        }
        const auto& visual = metadata::weapon_visual_info(
            result.projectile_weapon);
        result.projectile_draw_func = visual.draw_func_ids[0];
        result.projectile_color = visual.colors[0];
        result.projectile_collision_effect = visual.collision_effects[0];
        result.projectile_muzzle_effect = visual.muzzle_effects[0];
        result.projectile_speed = metadata::weapon_info(
            result.projectile_weapon).projectile_speed;
        result.projectile_lifetime = metadata::weapon_info(
            result.projectile_weapon).lifetime_seconds;
    }
    result.supported = result.location_volume.kind
            != scene::VolumeKind::Invalid
        && result.active_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

