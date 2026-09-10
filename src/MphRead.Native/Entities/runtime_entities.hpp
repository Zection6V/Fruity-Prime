#pragma once

#include <functional>
#include "Metadata/enemy_subroutines.hpp"
#include "Entities/Enemies/enemy_catalog.hpp"
#include "Messaging.hpp"
#include "Metadata/metadata.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Metadata/weapon_metadata.hpp"

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::players {
class PlayerEntity;
}

namespace fruityprime::runtime {

// Runtime types created by the managed Scene rather than read from a room's
// fixed entity table.  Keeping these separate from scene::EntityKind makes the
// native room reader useful for both static records and spawned gameplay
// objects.
enum class Kind : std::uint8_t {
    ItemInstance,
    BeamEffect,
    Bomb,
    EnemyInstance,
    EnemySpawner,
    Halfturret,
    Player,
    BeamProjectile
};

[[nodiscard]] std::string_view kind_name(Kind kind) noexcept;

// The gameplay session uses this state directly for its lightweight network
// projectile path.  The leading fields intentionally retain the old native
// aggregate layout; the additional fields expose the dynamic BeamProjectile
// state used by the full entity layer.
struct BeamProjectileEntity {
    net::Vec3 position;
    net::Vec3 direction{0.0F, 0.0F, 1.0F};
    std::uint8_t owner_slot = 0xff;
    std::uint8_t weapon = 0;
    std::uint16_t damage = 0;
    float speed = 24.0F;
    float lifetime = 0.0F;

    enum Flag : std::uint32_t {
        None = 0,
        Collided = 1u << 0,
        Charged = 1u << 1,
        Homing = 1u << 2,
        Ricochet = 1u << 3,
        SelfDamage = 1u << 4,
        ForceEffect = 1u << 5,
        Continuous = 1u << 6,
        Destroyable = 1u << 7,
        HasModel = 1u << 8,
        RadiusIndex1 = 1u << 9,
        RadiusIndex2 = 1u << 10,
        LifeDrain = 1u << 11,
        SurfaceCollision = 1u << 12,
        DestroyMuzzle = 1u << 13
    };

    std::uint32_t flags = None;
    net::Vec3 velocity{};
    net::Vec3 acceleration{};
    float age = 0.0F;
    float max_lifetime = 0.0F;
    net::Vec3 spawn_position{};

    // The managed BeamProjectileEntity keeps the previous point and a
    // ten-entry history for its textured trail.  Keep that state on the
    // shared projectile instead of reconstructing a one-frame line in the
    // Win32 renderer; headless/network clients can therefore retain the same
    // visual state when they become a frontend later.
    net::Vec3 back_position{};
    std::array<net::Vec3, 10> past_positions{};
    std::uint8_t draw_func_id = 0;
    net::Vec3 color{1.0F, 0.82F, 0.18F};
    std::uint8_t collision_effect = 0xff;
    std::uint8_t muzzle_effect = 0xff;
    std::uint8_t damage_dir_type = 0;
    std::uint8_t damage_interpolation = 0;
    std::uint8_t speed_interpolation = 0;
    std::uint8_t splash_damage_type = 0;
    std::uint8_t beam_kind = 0;
    float homing = 0.0F;
    net::Vec3 right{1.0F, 0.0F, 0.0F};
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    std::uint16_t headshot_damage = 0;
    std::uint16_t splash_damage = 0;
    float splash_radius = 0.0F;
    float max_distance = 0.0F;
    formats::Affliction afflictions = formats::Affliction::None;
    float speed_decay_time = 0.0F;
    float initial_speed = 0.0F;
    float final_speed = 0.0F;
    float damage_dir_magnitude = 0.0F;
    float ricochet_loss_h = 0.0F;
    float ricochet_loss_v = 0.0F;
    float cylinder_radius = 0.0F;

    // Enemy-owned beams do not have a player slot to attribute damage to.
    // Keep the owner entity on the shared projectile so the gameplay layer
    // can suppress self-collision and the renderer/effects layer can retain
    // the actual cartridge source entity.
    std::uint32_t owner_enemy_id = 0;

    // Advances a projectile without room collision.  Collision ownership
    // remains in gameplay::Session, while this method covers the shared
    // dynamic-entity lifetime/movement contract.
    [[nodiscard]] bool step(float seconds) noexcept;
    void reposition(net::Vec3 offset) noexcept;
};

using BeamProjectile = BeamProjectileEntity;

class Entity {
public:
    virtual ~Entity() = default;

    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] float age_seconds() const noexcept { return age_seconds_; }
    [[nodiscard]] const net::Vec3& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const net::Vec3& up() const noexcept { return up_; }
    [[nodiscard]] const net::Vec3& facing() const noexcept { return facing_; }
    [[nodiscard]] float alpha() const noexcept { return alpha_; }

    void set_active(bool value) noexcept { active_ = value; }
    void reposition(net::Vec3 offset) noexcept;

    // Returns true while the entity remains in the scene.
    [[nodiscard]] virtual bool process(float seconds) noexcept = 0;
    // Dynamic entities receive the same queued message boundary as fixed
    // room entities.  The default implementation handles generic activation
    // and deactivation; concrete entities extend it for damage/destruction.
    virtual void handle_message(const messaging::MessageInfo& message) noexcept;

protected:
    Entity(std::uint32_t id, Kind kind, net::Vec3 position) noexcept;

    void advance_age(float seconds) noexcept;

    std::uint32_t id_ = 0;
    Kind kind_ = Kind::Player;
    bool active_ = true;
    bool visible_ = true;
    float alpha_ = 1.0F;
    float age_seconds_ = 0.0F;
    net::Vec3 position_{};
    net::Vec3 up_{0.0F, 1.0F, 0.0F};
    net::Vec3 facing_{0.0F, 0.0F, 1.0F};
};

class ItemInstanceEntity final : public Entity {
public:
    ItemInstanceEntity(std::uint32_t id, std::int32_t item_type,
                       net::Vec3 position) noexcept;

    [[nodiscard]] std::int32_t item_type() const noexcept { return item_type_; }
    [[nodiscard]] std::int32_t parent_id() const noexcept { return parent_id_; }
    [[nodiscard]] float spin_degrees() const noexcept { return spin_degrees_; }
    [[nodiscard]] float float_offset() const noexcept { return float_offset_; }
    [[nodiscard]] float despawn_seconds() const noexcept {
        return despawn_seconds_;
    }
    [[nodiscard]] bool picked_up() const noexcept { return picked_up_; }

    void set_parent_id(std::int32_t parent_id) noexcept { parent_id_ = parent_id; }
    void set_despawn_seconds(float seconds) noexcept {
        despawn_seconds_ = seconds;
    }
    void on_picked_up() noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    std::int32_t item_type_ = -1;
    std::int32_t parent_id_ = -1;
    float spin_degrees_ = 0.0F;
    float float_offset_ = 0.0F;
    float despawn_seconds_ = -1.0F;
    bool picked_up_ = false;
};

struct FhItemInstanceEntityData {
    net::Vec3 position;
    formats::FhItemType item_type = formats::FhItemType::None;
};

class SpinningEntityBase : public Entity {
public:
    [[nodiscard]] float spin() const noexcept { return spin_; }
    [[nodiscard]] net::Vec3 spin_axis() const noexcept { return spin_axis_; }
    [[nodiscard]] int spin_model_index() const noexcept {
        return spin_model_index_;
    }
    [[nodiscard]] int float_model_index() const noexcept {
        return float_model_index_;
    }
    [[nodiscard]] float float_offset() const noexcept {
        return float_model_index_ < 0 ? 0.0F
            : (std::sin(spin_ * 0.01745329251994329577F) + 1.0F) / 8.0F;
    }
    [[nodiscard]] bool process(float seconds) noexcept override;

protected:
    SpinningEntityBase(std::uint32_t id, Kind kind, net::Vec3 position,
                       float spin_speed, net::Vec3 spin_axis,
                       int spin_model_index = -1,
                       int float_model_index = -1) noexcept;

private:
    static std::uint16_t next_item_rotation_;
    float spin_ = 0.0F;
    float spin_speed_ = 0.0F;
    net::Vec3 spin_axis_{};
    int spin_model_index_ = -1;
    int float_model_index_ = -1;
};

class FhItemEntity final : public SpinningEntityBase {
public:
    FhItemEntity(std::uint32_t id,
                 const FhItemInstanceEntityData& data) noexcept;
    [[nodiscard]] formats::FhItemType item_type() const noexcept {
        return item_type_;
    }
private:
    formats::FhItemType item_type_ = formats::FhItemType::None;
};

class BeamEffectEntity final : public Entity {
public:
    BeamEffectEntity(std::uint32_t id, std::int32_t type,
                     float lifespan_seconds, net::Vec3 position) noexcept;

    [[nodiscard]] std::int32_t type() const noexcept { return type_; }
    [[nodiscard]] float lifespan_seconds() const noexcept {
        return lifespan_seconds_;
    }
    [[nodiscard]] bool no_splat() const noexcept { return no_splat_; }
    void set_no_splat(bool value) noexcept { no_splat_ = value; }
    // BeamEffectEntityData.Type uses 0..2 for model-backed beam effects and
    // values >= 3 for a direct Scene.SpawnEffect(effectId, ...). Keep the
    // managed mapping here so renderers and tests do not have to duplicate it.
    [[nodiscard]] bool uses_model() const noexcept { return !model_name().empty(); }
    [[nodiscard]] std::string_view model_name() const noexcept;
    [[nodiscard]] std::int32_t spawned_effect_id() const noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    std::int32_t type_ = 0;
    float lifespan_seconds_ = 0.0F;
    bool no_splat_ = false;
};

enum class BombType : std::uint8_t {
    MorphBall,
    Stinglarva,
    Lockjaw
};

class BombEntity final : public Entity {
public:
    BombEntity(std::uint32_t id, BombType type, std::uint8_t owner_slot,
               net::Vec3 position, net::Vec3 velocity) noexcept;

    [[nodiscard]] BombType bomb_type() const noexcept { return bomb_type_; }
    [[nodiscard]] std::uint8_t owner_slot() const noexcept {
        return owner_slot_;
    }
    [[nodiscard]] float countdown_seconds() const noexcept {
        return countdown_seconds_;
    }
    [[nodiscard]] bool exploding() const noexcept { return exploding_; }
    [[nodiscard]] bool exploded() const noexcept { return exploded_; }
    [[nodiscard]] float radius() const noexcept { return radius_; }
    [[nodiscard]] std::uint16_t damage() const noexcept { return damage_; }

    void arm(float seconds) noexcept { countdown_seconds_ = seconds; }
    void set_radius(float radius) noexcept { radius_ = radius; }
    void set_damage(std::uint16_t damage) noexcept { damage_ = damage; }
    void trigger() noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    BombType bomb_type_ = BombType::MorphBall;
    std::uint8_t owner_slot_ = 0xff;
    net::Vec3 velocity_{};
    float countdown_seconds_ = 0.0F;
    float radius_ = 1.0F;
    float self_radius_ = 0.0F;
    std::uint16_t damage_ = 0;
    std::uint16_t enemy_damage_ = 0;
    bool exploding_ = false;
    bool exploded_ = false;
};

class EnemyInstanceEntity final : public Entity {
public:
    EnemyInstanceEntity(std::uint32_t id, std::uint8_t enemy_type,
                        net::Vec3 position) noexcept;

    [[nodiscard]] std::uint8_t enemy_type() const noexcept { return enemy_type_; }
    [[nodiscard]] const enemy::Profile& behavior_profile() const noexcept {
        return enemy::profile(enemy_type_);
    }
    [[nodiscard]] std::uint16_t health() const noexcept { return health_; }
    [[nodiscard]] std::uint16_t health_max() const noexcept {
        return health_max_;
    }
    [[nodiscard]] const metadata::EnemyInfo& metadata() const noexcept {
        return metadata::enemy_info(enemy_type_);
    }
    [[nodiscard]] std::array<metadata::Effectiveness, 9>
    effectiveness() const noexcept {
        return metadata::decode_effectiveness(metadata().effectiveness);
    }
    [[nodiscard]] std::uint16_t scan_id() const noexcept {
        return metadata().scan_id;
    }
    [[nodiscard]] std::uint16_t death_effect() const noexcept {
        return metadata().death_effect;
    }
    [[nodiscard]] std::uint8_t state_a() const noexcept { return state_a_; }
    [[nodiscard]] std::uint8_t state_b() const noexcept { return state_b_; }
    [[nodiscard]] std::uint32_t flags() const noexcept { return flags_; }

    enum Flag : std::uint32_t {
        CollidePlayer = 1u << 0,
        CollideBeam = 1u << 1,
        Invincible = 1u << 2,
        NoBombDamage = 1u << 3,
        Static = 1u << 4
    };

    void set_health(std::uint16_t health) noexcept { health_ = health; }
    void set_health_max(std::uint16_t health) noexcept { health_max_ = health; }
    void set_states(std::uint8_t state_a, std::uint8_t state_b) noexcept;

    // ---- the state machine ------------------------------------------
    // Every enemy in the cartridge is one: a state is an ordered list of
    // behaviours, the first whose predicate passes names the next state,
    // and the change takes effect on the frame after it is decided.  The
    // graphs are transliterated in Metadata/enemy_subroutines.hpp; what
    // lives here is the part EnemyInstanceEntity.cs owns for all of them.

    // The per-frame `_state1 = _state2; _subId = _state1` that makes a
    // decided transition take effect.  BaseProcess does this before the
    // enemy's own Process runs, which is why a behaviour that sets the
    // next state does not see it until the following frame.
    void AdvanceState() noexcept {
        state_a_ = state_b_;
        sub_id_ = state_a_;
    }

    // EnemyInstanceEntity.CallSubroutine.  `behavior` evaluates the
    // enemy's own BehaviorNN predicate by index -- C# holds an array of
    // delegates there, which is a switch here and nothing more.
    //
    // Returns whether any behaviour passed.  A state with an empty list,
    // or one where none pass, leaves the enemy where it is rather than
    // falling through to the first entry.
    [[nodiscard]] bool CallSubroutine(
        std::span<const metadata::EnemySubroutine> subroutines,
        const std::function<bool(std::uint8_t)>& behavior) noexcept;

    // EnemyInstanceEntity.CallStateProcess: run the current state's own
    // method.  Out of range is the caller's bug, and doing nothing is the
    // one response that cannot make it worse.
    void CallStateProcess(
        const std::function<void(std::uint8_t)>& states) const {
        if (states) {
            states(state_a_);
        }
    }

    [[nodiscard]] std::uint8_t sub_id() const noexcept { return sub_id_; }
    void set_flags(std::uint32_t flags) noexcept { flags_ = flags; }
    void set_velocity(net::Vec3 velocity) noexcept { velocity_ = velocity; }
    [[nodiscard]] bool take_damage(std::uint32_t damage) noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    std::uint8_t enemy_type_ = 0;
    // EnemyInstanceEntity._subId: which state's behaviour list to read.
    // It follows _state1 rather than being it, because a few enemies
    // change it between the state advance and the subroutine call.
    std::uint8_t sub_id_ = 0;
    std::uint8_t state_a_ = 0;
    std::uint8_t state_b_ = 0;
    std::uint32_t flags_ = CollidePlayer | CollideBeam;
    std::uint16_t health_ = 20;
    std::uint16_t health_max_ = 20;
    net::Vec3 velocity_{};
};

// Native counterpart of Entities/EnemySpawnEntity.cs.  The spawner owns the
// lifecycle counters and timing, while the scene/gameplay layer owns the
// actual enemy instances and can attach the returned spawn count to its own
// enemy factory.  Keeping this boundary small lets the same scheduler serve
// story rooms and a future First Hunt enemy implementation.
class EnemySpawnerEntity final : public Entity {
public:
    enum Flag : std::uint8_t {
        Suspended = 1u << 0,
        Active = 1u << 1,
        HasModel = 1u << 2,
        PlayAnimation = 1u << 3
    };

    struct TickResult {
        std::uint8_t spawned = 0;
        bool deactivated = false;
    };

    EnemySpawnerEntity(std::uint32_t id, std::uint8_t enemy_type,
                       net::Vec3 position, std::uint8_t spawn_total,
                       std::uint8_t spawn_limit, std::uint8_t spawn_count,
                       std::uint16_t cooldown_time,
                       std::uint16_t initial_cooldown, bool active,
                       bool always_active, float active_distance) noexcept;

    [[nodiscard]] std::uint8_t enemy_type() const noexcept {
        return enemy_type_;
    }
    [[nodiscard]] const enemy::Profile& behavior_profile() const noexcept {
        return enemy::profile(enemy_type_);
    }
    [[nodiscard]] std::uint32_t spawned_count() const noexcept {
        return spawned_count_;
    }
    [[nodiscard]] std::uint32_t active_count() const noexcept {
        return active_count_;
    }
    [[nodiscard]] std::uint8_t flags() const noexcept { return flags_; }
    [[nodiscard]] bool spawner_active() const noexcept {
        return (flags_ & Active) != 0;
    }
    [[nodiscard]] bool suspended() const noexcept {
        return (flags_ & Suspended) != 0;
    }
    [[nodiscard]] float active_distance() const noexcept {
        return active_distance_;
    }
    [[nodiscard]] float cooldown_seconds() const noexcept {
        return cooldown_seconds_;
    }

    // One fixed-timestep update.  The boolean range_node_ready corresponds
    // to the managed NodeRef gate; native node activation is supplied by the
    // scene once node runtime ownership is migrated.
    [[nodiscard]] TickResult tick(
        float seconds, std::span<const net::Vec3> player_positions,
        bool player_camera = true, bool range_node_ready = true) noexcept;

    // Message.Destroyed from a spawned enemy.  Out-of-range destruction frees
    // one total spawn slot as the managed implementation does.
    [[nodiscard]] bool on_enemy_destroyed(bool out_of_range = false) noexcept;
    void activate(bool value = true) noexcept;
    void set_has_model(bool value) noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    [[nodiscard]] bool complete_if_exhausted() noexcept;

    std::uint8_t enemy_type_ = 0;
    std::uint8_t spawn_total_ = 0;
    std::uint8_t spawn_limit_ = 0;
    std::uint8_t spawn_count_ = 0;
    std::uint8_t flags_ = Suspended;
    bool always_active_ = false;
    std::uint32_t spawned_count_ = 0;
    std::uint32_t active_count_ = 0;
    float active_distance_ = 0.0F;
    float cooldown_period_seconds_ = 0.0F;
    float cooldown_seconds_ = 0.0F;
};

class HalfturretEntity final : public Entity {
public:
    HalfturretEntity(std::uint32_t id, std::uint8_t owner_slot,
                     net::Vec3 position) noexcept;
    HalfturretEntity(std::uint32_t id, players::PlayerEntity& owner) noexcept;

    [[nodiscard]] std::uint8_t owner_slot() const noexcept {
        return owner_slot_;
    }
    [[nodiscard]] players::PlayerEntity* owner() const noexcept {
        return owner_;
    }
    [[nodiscard]] std::int32_t health() const noexcept { return health_; }
    [[nodiscard]] std::uint16_t time_since_damage() const noexcept {
        return time_since_damage_;
    }
    void set_time_since_damage(std::uint16_t value) noexcept {
        time_since_damage_ = value;
    }
    [[nodiscard]] players::PlayerEntity* target() const noexcept {
        return target_;
    }
    [[nodiscard]] float cooldown_factor() const noexcept {
        return cooldown_factor_;
    }
    [[nodiscard]] float frozen_seconds() const noexcept {
        return frozen_seconds_;
    }
    [[nodiscard]] float burn_seconds() const noexcept { return burn_seconds_; }

    void set_health(std::int32_t health) noexcept;
    void initialize_from_owner() noexcept;
    void take_damage(std::uint32_t damage,
                     players::PlayerEntity* attacker = nullptr) noexcept;
    void on_take_damage(std::uint32_t damage) noexcept;
    void on_take_damage(players::PlayerEntity* attacker,
                        std::uint32_t damage) noexcept;
    void on_frozen(float seconds = 2.5F) noexcept;
    void on_set_on_fire(float seconds = 5.0F) noexcept;

    // HalfturretEntity.ResetGroundedState: the turret is put back in the
    // air so the next frame re-tests what it is standing on.  Its owner
    // calls this after moving it.
    void reset_grounded_state() noexcept { grounded_ = false; }
    [[nodiscard]] bool grounded() const noexcept { return grounded_; }
    void set_grounded(bool grounded) noexcept { grounded_ = grounded; }

    // HalfturretEntity.Die: the owner is told first, so a player who is
    // still holding the halfturret flag drops it before the death effect
    // spawns.  Returns true when the death effect should be spawned --
    // dying twice must not spawn it twice.
    [[nodiscard]] bool die() noexcept;
    // Effect 216, "deathAlt", at the turret's position.
    static constexpr int DeathEffectId = 216;

    // HalfturretEntity.UpdateAim: the ballistic solve that points a
    // gravity-affected shot at a target.  It returns false when the target
    // is out of reach at this charge -- the aim vector is then the flattest
    // shot available rather than nothing, which is why the caller can fire
    // anyway.  `charge_level` is EquipInfo.ChargeLevel.
    [[nodiscard]] static bool update_aim(
        net::Vec3 muzzle_position, net::Vec3 target_position,
        const metadata::weapon_table::WeaponInfo& weapon,
        std::uint16_t charge_level, net::Vec3& aim_vector) noexcept;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    players::PlayerEntity* owner_ = nullptr;
    players::PlayerEntity* target_ = nullptr;
    std::uint8_t owner_slot_ = 0xff;
    std::int32_t health_ = 0;
    std::uint16_t time_since_damage_ = 0xffff;
    std::uint16_t time_since_frozen_ = 0;
    std::uint16_t freeze_timer_ = 0;
    std::uint16_t burn_timer_ = 0;
    std::uint16_t target_timer_ = 0;
    std::uint16_t cooldown_timer_ = 0;
    float y_speed_ = 0.0F;
    net::Vec3 aim_vector_{};
    float cooldown_factor_ = 1.5F;
    float frozen_seconds_ = 0.0F;
    float burn_seconds_ = 0.0F;
    bool grounded_ = false;
};

class EntityPool {
public:
    template <typename T, typename... Arguments>
    T& emplace(Arguments&&... arguments) {
        auto value = std::make_unique<T>(
            std::forward<Arguments>(arguments)...);
        T& result = *value;
        entities_.push_back(std::move(value));
        return result;
    }

    [[nodiscard]] std::size_t process(float seconds) noexcept;
    // Dispatches to a target ID, or to every pooled entity when target < 0.
    // The return value counts entities that received the message.
    [[nodiscard]] std::size_t dispatch(
        const messaging::MessageInfo& message) noexcept;
    void clear() noexcept { entities_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    [[nodiscard]] const std::vector<std::unique_ptr<Entity>>& entities()
        const noexcept {
        return entities_;
    }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
};

} // namespace fruityprime::runtime
