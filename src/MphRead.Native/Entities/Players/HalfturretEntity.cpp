// Native counterpart of src/MphRead/Entities/Players/HalfturretEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Assets/model_catalog.hpp"
#include "Formats/model_instance.hpp"
#include "../runtime_entity_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fruityprime::runtime {

using namespace detail;

namespace {

std::uint16_t frame_count(float seconds) noexcept {
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        return 0;
    }
    return static_cast<std::uint16_t>(std::clamp<long>(
        std::lround(seconds * 60.0F), 1, 0xffff));
}

void add_frames_saturated(std::uint16_t& value,
                          std::uint16_t amount) noexcept {
    value = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(value) + amount, 0xffff));
}

} // namespace

HalfturretEntity::HalfturretEntity(std::uint32_t id, std::uint8_t owner_slot,
                                   net::Vec3 position) noexcept
    : Entity(id, Kind::Halfturret, position), owner_slot_(owner_slot) {}

HalfturretEntity::HalfturretEntity(std::uint32_t id,
                                   players::PlayerEntity& owner) noexcept
    : Entity(id, Kind::Halfturret, {}), owner_(&owner),
      owner_slot_(static_cast<std::uint8_t>(owner.SlotIndex())) {}

HalfturretEntity::~HalfturretEntity() = default;

void HalfturretEntity::bind_asset_store(
    const assets::Store* assets) noexcept {
    asset_store_ = assets;
}

void HalfturretEntity::create() {
    if (asset_store_ == nullptr) {
        throw std::logic_error(
            "HalfturretEntity.Create requires the active asset store");
    }

    // EntityBase.SetUpModel calls Read.GetModelInstance, then sets animation
    // zero before adding the instance to _models.  Keep the same order for
    // both models instead of retaining only their metadata names.
    auto turret = assets::try_load_named_model(*asset_store_, model_name());
    if (!turret.has_value()) {
        throw std::runtime_error(
            "HalfturretEntity.Create could not load WeavelAlt_Turret_lod0");
    }
    turret_model_ = std::make_unique<model::File>(std::move(turret->model));
    turret_instance_ = std::make_unique<model::ModelInstance>(*turret_model_);
    turret_instance_->set_animation(0);

    base_node_index_ = turret_model_->get_node_index_by_name("TurretBase");
    if (base_node_index_ < 0
        || static_cast<std::size_t>(base_node_index_)
               >= turret_model_->nodes().size()) {
        throw std::runtime_error(
            "HalfturretEntity.Create could not resolve TurretBase");
    }
    base_node_parent_index_ = turret_model_->nodes()[
        static_cast<std::size_t>(base_node_index_)].parent_index();
    if (base_node_parent_index_ < 0
        || static_cast<std::size_t>(base_node_parent_index_)
               >= turret_model_->nodes().size()) {
        throw std::runtime_error(
            "HalfturretEntity.Create could not resolve TurretBase parent");
    }

    auto alt_ice = assets::try_load_named_model(*asset_store_, ice_model_name());
    if (!alt_ice.has_value()) {
        throw std::runtime_error(
            "HalfturretEntity.Create could not load alt_ice");
    }
    alt_ice_model_ = std::make_unique<model::File>(
        std::move(alt_ice->model));
    alt_ice_instance_ = std::make_unique<model::ModelInstance>(*alt_ice_model_);
    alt_ice_instance_->set_animation(0);
    created_ = true;
}

void HalfturretEntity::init_scene_entity() noexcept {
    // Scene.InitEntity's renderer-independent work is the existence of the
    // already-created model instances.  The Win32 scene callback performs
    // texture/list registration against these exact instances.
    scene_initialized_ = true;
}

void HalfturretEntity::initialize_from_owner() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    const auto owner_position = owner_->Position();
    const float min_y = static_cast<float>(owner_->Values().MinPickupHeight)
        / 4096.0F;
    const net::Vec3 position{owner_position.x,
                             owner_position.y + min_y + 0.45F,
                             owner_position.z};
    reposition({position.x - position_.x, position.y - position_.y,
                position.z - position_.z});
    const int health = owner_->Health();
    if (health > 1) {
        health_ = health / 2;
        owner_->Health(health - health_);
    } else {
        health_ = 1;
    }
    active_ = true;
    grounded_ = false;
    y_speed_ = 0.0F;
    target_ = nullptr;
    target_timer_ = 0;
    cooldown_timer_ = 0;
    time_since_damage_ = 0xffff;
    time_since_frozen_ = 0;
    freeze_timer_ = 0;
    burn_timer_ = 0;
    cooldown_factor_ = 1.5F;
    frozen_seconds_ = 0.0F;
    burn_seconds_ = 0.0F;
}

void HalfturretEntity::set_health(std::int32_t health) noexcept {
    health_ = std::max(0, health);
    if (health_ == 0) {
        active_ = false;
    }
}

void HalfturretEntity::take_damage(
    std::uint32_t damage, players::PlayerEntity* attacker) noexcept {
    if (!active_ || damage == 0) {
        return;
    }
    on_take_damage(attacker, damage);
    if (damage >= static_cast<std::uint32_t>(health_)) {
        static_cast<void>(die());
    } else {
        health_ -= static_cast<std::int32_t>(damage);
        time_since_damage_ = 0;
    }
}

void HalfturretEntity::on_take_damage(std::uint32_t damage) noexcept {
    on_take_damage(nullptr, damage);
}

void HalfturretEntity::on_take_damage(
    players::PlayerEntity* attacker, std::uint32_t damage) noexcept {
    target_ = attacker;
    target_timer_ = 30 * 2;
    cooldown_factor_ = std::max(0.7F, cooldown_factor_
        - static_cast<float>(damage) * 61.0F);
}

void HalfturretEntity::on_frozen(float seconds) noexcept {
    const std::uint16_t requested = frame_count(seconds);
    if (time_since_frozen_ > 60 * 2) {
        freeze_timer_ = requested;
    } else if (freeze_timer_ < 15 * 2) {
        freeze_timer_ = 15 * 2;
    }
    frozen_seconds_ = static_cast<float>(freeze_timer_) / 60.0F;
}

void HalfturretEntity::on_set_on_fire(float seconds) noexcept {
    burn_timer_ = frame_count(seconds);
    burn_seconds_ = static_cast<float>(burn_timer_) / 60.0F;
}

void HalfturretEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 7)) {
        take_damage(message.parameter1 > 0
                        ? static_cast<std::uint32_t>(message.parameter1)
                        : 1u);
        return;
    }
    if (cartridge_message(message, 48)) {
        active_ = true;
        return;
    }
    if (cartridge_message(message, 51)) {
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

bool HalfturretEntity::process(float seconds) noexcept {
    if (!active_ || health_ == 0
        || (owner_ != nullptr && !owner_->HasHalfturret())) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    const std::uint16_t frames = frame_count(seconds);
    if (burn_timer_ > 0) {
        const std::uint16_t elapsed = std::min(burn_timer_, frames);
        for (std::uint16_t frame = 0; frame < elapsed; ++frame) {
            --burn_timer_;
            if (burn_timer_ % (8 * 2) == 0 && owner_ != nullptr) {
                owner_->TakeDamage(
                    1U,
                    static_cast<formats::DamageFlags>(
                        static_cast<std::int32_t>(formats::DamageFlags::NoSfx)
                        | static_cast<std::int32_t>(formats::DamageFlags::Burn)
                        | static_cast<std::int32_t>(formats::DamageFlags::NoDmgInvuln)
                        | static_cast<std::int32_t>(formats::DamageFlags::Halfturret)));
            }
        }
    }
    burn_seconds_ = static_cast<float>(burn_timer_) / 60.0F;

    if (freeze_timer_ > 0) {
        const std::uint16_t elapsed = std::min(freeze_timer_, frames);
        freeze_timer_ = static_cast<std::uint16_t>(freeze_timer_ - elapsed);
        time_since_frozen_ = 0;
    } else {
        constexpr float CooldownRecoveryPerSecond = 0.225F;
        if (cooldown_factor_ < 1.5F) {
            cooldown_factor_ = std::min(
                cooldown_factor_ + CooldownRecoveryPerSecond * seconds,
                1.5F);
        } else if (cooldown_factor_ > 1.5F) {
            cooldown_factor_ = std::max(
                cooldown_factor_ - CooldownRecoveryPerSecond * seconds,
                1.5F);
        }
        if (target_timer_ > frames) {
            target_timer_ = static_cast<std::uint16_t>(target_timer_ - frames);
        } else {
            target_timer_ = 0;
            target_ = nullptr;
        }
        if (target_ == nullptr && owner_ != nullptr) {
            float closest_distance = 15.0F * 15.0F;
            for (players::PlayerEntity* player
                 : players::PlayerEntity::Players()) {
                if (player == nullptr || player == owner_
                    || !player->GetTargetable()
                    || player->TeamIndex() == owner_->TeamIndex()
                    || player->CurAlpha() < 6.0F / 31.0F) {
                    continue;
                }
                const net::Vec3 other = player->Position();
                const float x = other.x - position_.x;
                const float y = other.y - position_.y;
                const float z = other.z - position_.z;
                const float distance = x * x + y * y + z * z;
                if (distance < closest_distance) {
                    closest_distance = distance;
                    target_ = player;
                }
            }
        }
    }
    frozen_seconds_ = static_cast<float>(freeze_timer_) / 60.0F;
    if (time_since_frozen_ != 0xffff) {
        add_frames_saturated(time_since_frozen_, frames);
    }
    if (time_since_damage_ != 0xffff) {
        add_frames_saturated(time_since_damage_, frames);
    }
    return active_;
}

bool HalfturretEntity::die() noexcept {
    // The owner is told whether or not there is any health left: the flag the
    // halfturret carries has to come off the player either way.
    if (owner_ != nullptr) {
        owner_->OnHalfturretDied();
    }
    if (health_ <= 0) {
        return false;
    }
    health_ = 0;
    active_ = false;
    return true;
}

bool HalfturretEntity::update_aim(
    net::Vec3 muzzle_position, net::Vec3 target_position,
    const metadata::weapon_table::WeaponInfo& weapon,
    std::uint16_t charge_level, net::Vec3& aim_vector) noexcept {
    // The charge is measured in doubled frames, so the thresholds are too.
    float charge_percent = 0.0F;
    const auto min_charge = static_cast<float>(weapon.min_charge) * 2.0F;
    const auto full_charge = static_cast<float>(weapon.full_charge) * 2.0F;
    if (metadata::weapon_table::has_flag(
            weapon.flags, metadata::weapon_table::WeaponFlags::CanCharge)
        && static_cast<float>(charge_level) >= min_charge
        && full_charge > min_charge) {
        charge_percent = (static_cast<float>(charge_level) - min_charge)
            / (full_charge - min_charge);
    }
    aim_vector = {target_position.x - muzzle_position.x,
                  target_position.y - muzzle_position.y,
                  target_position.z - muzzle_position.z};
    const float horizontal_squared =
        aim_vector.x * aim_vector.x + aim_vector.z * aim_vector.z;
    const float horizontal = std::sqrt(horizontal_squared);
    const float uncharged_speed =
        static_cast<float>(weapon.uncharged_speed) / 4096.0F;
    const float speed =
        (static_cast<float>(weapon.min_charge_speed) / 4096.0F
         - uncharged_speed) * charge_percent;
    const float uncharged_gravity =
        static_cast<float>(weapon.uncharged_gravity) / 4096.0F;
    const float gravity =
        (static_cast<float>(weapon.min_charge_gravity) / 4096.0F
         - uncharged_gravity) * charge_percent;
    const float total_speed = uncharged_speed + speed;
    // How far the shot drops over the horizontal distance, in units of that
    // distance.  A weapon with no gravity leaves this at zero and the aim
    // vector is the straight line.
    const float drop = total_speed == 0.0F
        ? 0.0F
        : horizontal_squared * (uncharged_gravity + gravity)
            / (total_speed * total_speed);
    float scaled_drop = drop;
    float half_drop = drop / 2.0F;
    bool result = true;
    // The 1/4096 comparisons are the cartridge's own test for "not zero" in
    // fixed point; a float equality would answer differently.
    if (half_drop >= 1.0F / 4096.0F || half_drop <= -1.0F / 4096.0F) {
        // The cartridge truncates to fixed point and nudges odd values down,
        // which keeps the square root below from going imaginary on the exact
        // boundary.
        if ((static_cast<int>(scaled_drop * 4096.0F) & 1) == 1) {
            scaled_drop -= 1.0F / 4096.0F;
            half_drop = scaled_drop / 2.0F;
        }
        const float discriminant = horizontal_squared
            - 4.0F * half_drop * (half_drop - aim_vector.y);
        if (discriminant > 0.0F) {
            aim_vector.y =
                (std::sqrt(discriminant) - horizontal) / scaled_drop
                * horizontal;
        } else if (discriminant > -1.0F / 4096.0F) {
            aim_vector.y = -horizontal / scaled_drop * horizontal;
        } else {
            // Out of reach: the flattest shot the weapon can make is used, and
            // the caller is told it will fall short.
            aim_vector.y = horizontal;
            result = false;
        }
    }
    const float length_squared = aim_vector.x * aim_vector.x
        + aim_vector.y * aim_vector.y + aim_vector.z * aim_vector.z;
    if (length_squared > 0.0F) {
        const float inverse = 1.0F / std::sqrt(length_squared);
        aim_vector = {aim_vector.x * inverse, aim_vector.y * inverse,
                      aim_vector.z * inverse};
    } else {
        // Standing exactly on the target: any direction will do, and the
        // managed code picks +X.
        aim_vector = {1.0F, 0.0F, 0.0F};
    }
    return result;
}

} // namespace fruityprime::runtime
