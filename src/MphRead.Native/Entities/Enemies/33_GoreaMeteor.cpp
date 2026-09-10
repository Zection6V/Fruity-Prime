// Native counterpart of src/MphRead/Entities/Enemies/33_GoreaMeteor.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "33_GoreaMeteor.hpp"
#include "31_Gorea2.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "Utility/rng.hpp"
#include "enemy_scene.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::gameplay {

namespace {

constexpr float Pi33 = 3.14159265358979323846F;

// The four effects a meteor can leave behind, by the numbers the
// cartridge gives them.
constexpr std::uint16_t GoreaMeteorEffect = 79u;         // goreaMeteor
constexpr std::uint16_t GoreaMeteorDamageEffect = 176u;  // goreaMeteorDamage
constexpr std::uint16_t GoreaMeteorDestroyEffect = 177u; // goreaMeteorDestroy
constexpr std::uint16_t GoreaMeteorHitEffect = 178u;     // goreaMeteorHit

// Native counterpart of Enemy33Entity.
//
// A meteor Gorea throws.  It homes, slowly, and it is shootable -- eight
// energy, and it drops something when it dies, which is what makes
// shooting them down worth the shots rather than just dodging.
//
// It carries two fuses and only one of them is running at a time.  While
// it has not noticed anybody the long one ticks; the moment a player
// comes within two units it switches to the short one, which is less than
// half as long, and starts flashing.  So the flashing is the warning, and
// running from a meteor that has not seen you buys nothing at all.
class Enemy33Entity final {
public:
    Enemy33Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* target) noexcept
        : scene_(scene), agent_(agent), target_(target) {}

    // Enemy33Entity.EnemyInitialize.
    void EnemyInitialize();

    // Enemy33Entity.InitializePosition.
    void InitializePosition(net::Vec3 position);

    // Enemy33Entity.EnemyProcess.
    void EnemyProcess();

    // Enemy33Entity.EnemyTakeDamage.  Returns whether the damage is to be
    // ignored, which for a meteor is always: it handles its own dying.
    [[nodiscard]] bool EnemyTakeDamage();

    // Enemy33Entity.Explode.  Public because the damage path finishes a
    // meteor off from outside, and because Gorea's own retreat clears the
    // ones still in the air.
    void Explode(std::uint16_t effect_id);

private:
    // Enemy33Entity.UpdateRotation, UpdateSpeed, CheckCollision,
    // CheckHitPlayer and UpdatePosition -- the five halves of its process.
    void UpdateRotation();
    void UpdateSpeed();
    void CheckCollision();
    void CheckHitPlayer();
    void UpdatePosition();

    // Enemy33Entity.Func2140E44: what a hit does besides hurt.  It resets
    // the red and latches the flash on, and the managed name is kept
    // because nobody has worked out what to call it.
    void Func2140E44();

    // Enemy33Entity.SpawnItemDrop and CheckExplosionDamage.
    void SpawnItemDrop();
    void CheckExplosionDamage();

    void State00();
    void State01();
    void State02();
    void State03();

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01() const noexcept { return true; }
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03() const noexcept;

    [[nodiscard]] bool CallSubroutine();
    void CallStateProcess();

    const EnemyScene& scene_;
    EnemyState& agent_;
    net::PlayerState* target_;
};

void Enemy33Entity::EnemyInitialize() {
    agent_.visible = true;
    agent_.invulnerable = false;
    agent_.body_radius = 1.0F;
    agent_.health = agent_.health_max = 8;
    agent_.state = agent_.next_state = agent_.sub_id = 0;
    agent_.gorea_meteor_base_position = agent_.position;
    agent_.gorea_meteor_previous_position = agent_.position;
    agent_.gorea_meteor_effect_up = {0.0F, 1.0F, 0.0F};
    agent_.gorea_meteor_effect_facing = {0.0F, 0.0F, 1.0F};
    // Two seconds' worth of blast radius, an eighth of a unit a frame,
    // fifteen damage and a unit of shove.
    agent_.gorea_meteor_long_fuse = 390u * 2u;   // todo: FPS stuff
    agent_.gorea_meteor_short_fuse = 150u * 2u;  // todo: FPS stuff
    agent_.gorea_meteor_rotation = 0.0F;
    agent_.gorea_meteor_shake_timer = 0;
    agent_.gorea_meteor_flash_interval = 5u * 2u;  // todo: FPS stuff
    agent_.gorea_meteor_flash_timer = 0;
    agent_.gorea_meteor_flashing = false;
    agent_.gorea_meteor_time_since_damage = 510;
    // Two chances in five of health, three in five of nothing at all --
    // and the middle two weights are zero, so a meteor never drops ammo
    // or missiles however the roll goes.  Kept as four numbers because
    // that is what the cartridge authored.
    agent_.gorea_meteor_item_chance1 = 40;
    agent_.gorea_meteor_item_chance2 = 0;
    agent_.gorea_meteor_item_chance3 = 0;
    agent_.gorea_meteor_item_chance4 = 60;
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(GoreaMeteorEffect, agent_.position, agent_.id);
    }
}

void Enemy33Entity::InitializePosition(const net::Vec3 position) {
    agent_.position = position;
    agent_.gorea_meteor_base_position = position;
    agent_.gorea_meteor_previous_position = position;
}

void Enemy33Entity::EnemyProcess() {
    if (!agent_.visible) {
        return;
    }
    UpdateRotation();
    CallStateProcess();
    UpdateSpeed();
    CheckCollision();
    CheckHitPlayer();
    UpdatePosition();
}

// It tumbles about the axis across its own facing and up, which is why a
// meteor rolls sideways as it comes at you rather than spinning on the
// spot.
void Enemy33Entity::UpdateRotation() {
    agent_.gorea_meteor_rotation += 12.0F / 2.0F;  // todo: FPS stuff
    if (agent_.gorea_meteor_rotation >= 360.0F) {
        agent_.gorea_meteor_rotation -= 360.0F;
    }
    const net::Vec3 up = agent_.gorea_meteor_effect_up;
    const net::Vec3 facing = agent_.gorea_meteor_effect_facing;
    const net::Vec3 axis = normalized_or(
        net::Vec3{up.y * facing.z - up.z * facing.y,
                  up.z * facing.x - up.x * facing.z,
                  up.x * facing.y - up.y * facing.x},
        {0.0F, 1.0F, 0.0F});
    const float angle = agent_.gorea_meteor_rotation * Pi33 / 180.0F;
    agent_.up = rotate_about_axis(up, axis, angle);
    agent_.facing = rotate_about_axis(facing, axis, angle);
}

void Enemy33Entity::UpdateSpeed() {
    if (target_ == nullptr) {
        return;
    }
    net::Vec3 speed{target_->position.x - agent_.position.x,
                    target_->position.y - agent_.position.y,
                    target_->position.z - agent_.position.z};
    if (length_squared(speed) > 1.0F / 128.0F) {
        speed = normalized_or(speed, agent_.gorea_meteor_effect_facing);
        agent_.gorea_meteor_effect_facing = speed;
        // Enemy31Entity.Func21418EC: the up that keeps the pair square
        // once the facing has turned.
        agent_.gorea_meteor_effect_up = enemy::module_31_gorea_2::Func21418EC(
            agent_.gorea_meteor_effect_facing, agent_.gorea_meteor_effect_up);
        speed = multiply(speed, 0.125F);
    }
    agent_.velocity = multiply(speed, 0.5F);  // todo: FPS stuff
}

// A meteor that has passed through something explodes on it.  The test is
// against where it was rather than where it is, so one moving fast enough
// to jump a wall in a frame still hits it.
void Enemy33Entity::CheckCollision() {
    if (agent_.health == 0) {
        return;
    }
    const net::Vec3 travel = subtract(agent_.gorea_meteor_previous_position,
                                      agent_.position);
    if (length_squared(travel) <= 1.0F / 128.0F) {
        return;
    }
    if (scene_.Blocked
        && scene_.Blocked(agent_.gorea_meteor_previous_position,
                          agent_.position, 0.01F)) {
        CheckExplosionDamage();
        if (scene_.SpawnEffect) {
            scene_.SpawnEffect(GoreaMeteorHitEffect, agent_.position,
                               agent_.id);
        }
    }
}

void Enemy33Entity::CheckHitPlayer() {
    if (agent_.health == 0 || target_ == nullptr) {
        return;
    }
    const float reach = agent_.body_radius + 0.45F;
    if (distance_squared(agent_.position, target_->position)
            <= reach * reach) {
        Explode(GoreaMeteorDamageEffect);
    }
}

// The shake is a wobble about the base position rather than a change of
// course: a meteor that has been shot jitters where it is and keeps
// coming.
void Enemy33Entity::UpdatePosition() {
    agent_.gorea_meteor_base_position =
        add(agent_.gorea_meteor_base_position, agent_.velocity);
    if (agent_.gorea_meteor_shake_timer > 0) {
        const auto jitter = []() {
            return (static_cast<float>(utility::get_random_int2(512u))
                    - 256.0F) / 4096.0F;
        };
        agent_.position = {agent_.gorea_meteor_base_position.x + jitter(),
                           agent_.gorea_meteor_base_position.y + jitter(),
                           agent_.gorea_meteor_base_position.z + jitter()};
        --agent_.gorea_meteor_shake_timer;
    }
}

bool Enemy33Entity::EnemyTakeDamage() {
    if (agent_.health > 0) {
        agent_.gorea_meteor_shake_timer = 30u * 2u;  // todo: FPS stuff
        if (scene_.SpawnEffect) {
            scene_.SpawnEffect(GoreaMeteorDamageEffect, agent_.position,
                               agent_.id);
        }
        Func2140E44();
    } else {
        SpawnItemDrop();
        Explode(GoreaMeteorDestroyEffect);
    }
    return false;
}

void Enemy33Entity::Func2140E44() {
    agent_.gorea_meteor_time_since_damage = 0;
    agent_.gorea_meteor_flashing = true;
}

void Enemy33Entity::SpawnItemDrop() {
    bool spawn = true;
    std::uint8_t item_type = 0xff;
    const std::uint32_t chance1 = agent_.gorea_meteor_item_chance1;
    const std::uint32_t chance2 = chance1 + agent_.gorea_meteor_item_chance2;
    const std::uint32_t chance3 = chance2 + agent_.gorea_meteor_item_chance3;
    const std::uint32_t chance4 = chance3 + agent_.gorea_meteor_item_chance4;
    if (chance4 == 0) {
        return;
    }
    const std::uint32_t roll = utility::get_random_int2(chance4);
    if (roll >= chance3) {
        spawn = false;
    } else if (roll >= chance2) {
        item_type = static_cast<std::uint8_t>(ItemType::UASmall);
    } else if (roll >= chance1) {
        item_type = static_cast<std::uint8_t>(ItemType::MissileSmall);
    } else {
        item_type = static_cast<std::uint8_t>(ItemType::HealthSmall);
    }
    if (spawn && scene_.DropItem) {
        // Five seconds before it goes away again, which is the only
        // window there is to collect it.
        scene_.DropItem(agent_.position, item_type, 300u * 2u);
    }
}

void Enemy33Entity::Explode(const std::uint16_t effect_id) {
    CheckExplosionDamage();
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(effect_id, agent_.position, agent_.id);
    }
    agent_.health = 0;
    agent_.visible = false;
    agent_.invulnerable = true;
    agent_.velocity = {};
    // Put out of the world rather than removed, because the record still
    // has to be reaped by whoever counts Gorea's meteors.
    agent_.position.y = 524288.0F;
}

// The blast.  Full damage to somebody the meteor actually touched, and
// falling off with distance for anyone merely near -- and nothing at all
// through a wall, which is why taking cover from one works.
void Enemy33Entity::CheckExplosionDamage() {
    if (target_ == nullptr) {
        return;
    }
    net::Vec3 to_target{target_->position.x - agent_.position.x,
                        target_->position.y - agent_.position.y,
                        target_->position.z - agent_.position.z};
    const float distance = std::sqrt(std::max(0.0F,
                                              length_squared(to_target)));
    // Two units, which is twice the meteor's own body.
    constexpr float BlastRadius = 2.0F;
    if (distance >= BlastRadius) {
        return;
    }
    if (scene_.Blocked
        && scene_.Blocked(agent_.gorea_meteor_previous_position,
                          target_->position, 0.01F)) {
        return;
    }
    std::uint32_t damage = 15;
    float force = 1.0F;
    const float reach = agent_.body_radius + 0.45F;
    const bool touching = distance_squared(agent_.position,
                                           target_->position)
        <= reach * reach;
    if (!touching) {
        const float factor = std::clamp(distance / BlastRadius, 0.0F, 1.0F);
        damage = static_cast<std::uint32_t>(
            static_cast<float>(damage) - static_cast<float>(damage) * factor);
        // The cartridge stores the shove through an integer, so a player
        // at the edge of the blast is pushed by nothing rather than by a
        // little.  Kept, because it is what happens.
        force = static_cast<float>(static_cast<int>(force - force * factor));
    }
    const net::Vec3 direction = distance > 1.0F / 128.0F
        ? multiply(to_target, 1.0F / distance)
        : net::Vec3{0.0F, 1.0F, 0.0F};
    target_->speed = add(target_->speed, multiply(direction, force));
    if (scene_.ContactDamage) {
        scene_.ContactDamage(agent_, target_->slot_index, damage);
    }
}

void Enemy33Entity::State00() {
    static_cast<void>(CallSubroutine());
}

void Enemy33Entity::State01() {
    static_cast<void>(CallSubroutine());
}

// The flash.  It is not decoration: state two is the state a meteor is in
// once it has noticed somebody, and the pulse is the only sign that the
// short fuse is now the one running.
void Enemy33Entity::State02() {
    if (agent_.gorea_meteor_flash_interval > 0) {  // always true
        if (agent_.gorea_meteor_flash_timer != 0) {
            --agent_.gorea_meteor_flash_timer;
        }
        if (agent_.gorea_meteor_flash_timer == 0) {
            agent_.gorea_meteor_flash_timer =
                agent_.gorea_meteor_flash_interval;
            agent_.gorea_meteor_flashing = !agent_.gorea_meteor_flashing;
            if (agent_.gorea_meteor_flashing) {
                Func2140E44();
            }
        }
    }
    static_cast<void>(CallSubroutine());
}

void Enemy33Entity::State03() {
    static_cast<void>(CallSubroutine());
}

// The short fuse, which runs only in state two.
bool Enemy33Entity::Behavior00() {
    if (agent_.gorea_meteor_short_fuse > 0) {
        --agent_.gorea_meteor_short_fuse;
    }
    if (agent_.gorea_meteor_short_fuse == 0) {
        Explode(GoreaMeteorHitEffect);
        return true;
    }
    return false;
}

// The long one, which runs only in state nought.  The managed class notes
// this is Behavior00 with the other timer, and it is.
bool Enemy33Entity::Behavior02() {
    if (agent_.gorea_meteor_long_fuse > 0) {
        --agent_.gorea_meteor_long_fuse;
    }
    if (agent_.gorea_meteor_long_fuse == 0) {
        Explode(GoreaMeteorHitEffect);
        return true;
    }
    return false;
}

// Noticing somebody: the player's own body within the blast radius, not
// within the meteor's.  So a meteor commits to you from further away than
// it can actually hit you from.
bool Enemy33Entity::Behavior03() const noexcept {
    if (target_ == nullptr) {
        return false;
    }
    constexpr float BlastRadius = 2.0F;
    const float reach = BlastRadius + 0.45F;
    return distance_squared(agent_.position, target_->position)
        <= reach * reach;
}

bool Enemy33Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy33Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            default: return false;
            }
        });
}

void Enemy33Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State00(); break;
    case 1: State01(); break;
    case 2: State02(); break;
    case 3: State03(); break;
    default: break;
    }
}

} // namespace

void Session::detonate_gorea_meteor(EnemyState& agent,
                                    const std::uint16_t effect_id) {
    net::PlayerState* target = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest) {
            nearest = distance;
            target = &player;
        }
    }
    EnemyScene scene = build_gorea_meteor_scene(agent);
    Enemy33Entity(scene, agent, target).Explode(effect_id);
}

bool Session::gorea_meteor_take_damage(EnemyState& agent) {
    net::PlayerState* target = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest) {
            nearest = distance;
            target = &player;
        }
    }
    EnemyScene scene = build_gorea_meteor_scene(agent);
    return Enemy33Entity(scene, agent, target).EnemyTakeDamage();
}

EnemyScene Session::build_gorea_meteor_scene(const EnemyState& agent) {
    EnemyScene scene = build_enemy_scene(
        agent.gorea_meteor_previous_position);
    scene.SpawnEffect = [this](std::uint32_t effect, net::Vec3 position,
                               std::uint32_t owner) {
        static_cast<void>(spawn_effect(
            static_cast<std::uint16_t>(effect), position, {0.0F, 0.0F, 1.0F},
            owner));
    };
    scene.DropItem = [this](net::Vec3 position, std::uint8_t item_type,
                            std::uint32_t despawn_frames) {
        static_cast<void>(despawn_frames);
        spawn_item_drop(static_cast<ItemType>(item_type), position);
    };
    return scene;
}

void Session::update_gorea_meteor(EnemyState& agent) {
    // A meteor whose fuse has run out keeps its record for one more pass,
    // so whoever counts Gorea's meteors sees it go.
    const auto release_parent_slot = [this, &agent]() {
        if (agent.parent_enemy_id == 0) {
            return;
        }
        const auto parent = std::find_if(
            enemies_.begin(), enemies_.end(),
            [parent_id = agent.parent_enemy_id](const EnemyState& value) {
                return value.id == parent_id
                    && value.enemy_type == static_cast<std::uint8_t>(
                        formats::EnemyType::Gorea2);
            });
        if (parent != enemies_.end() && parent->gorea_meteor_count > 0) {
            --parent->gorea_meteor_count;
        }
    };

    if (!agent.active) {
        return;
    }
    if (agent.health == 0) {
        release_parent_slot();
        agent.active = false;
        return;
    }
    if (!agent.visible) {
        agent.velocity = {};
        return;
    }

    net::PlayerState* target = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest) {
            nearest = distance;
            target = &player;
        }
    }
    if (target != nullptr) {
        agent.target_slot = target->slot_index;
    }

    // EnemyInstanceEntity.BaseProcess.  A meteor moves itself in
    // UpdatePosition rather than through the shared step, because what it
    // moves is its base position and the body wobbles around that.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    agent.gorea_meteor_previous_position = agent.position;

    EnemyScene scene = build_gorea_meteor_scene(agent);
    Enemy33Entity(scene, agent, target).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_33_gorea_meteor::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_33_gorea_meteor {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy33Entity(empty, agent, nullptr).EnemyInitialize();
}

void InitializePosition(gameplay::EnemyState& agent,
                        const net::Vec3 position) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy33Entity(empty, agent, nullptr)
        .InitializePosition(position);
}

} // namespace fruityprime::enemy::module_33_gorea_meteor
