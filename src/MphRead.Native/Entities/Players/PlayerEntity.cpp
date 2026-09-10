// Native counterpart of src/MphRead/Entities/Players/PlayerEntity.cs.
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/Players/PlayerProcess.hpp"
#include "Entities/gameplay.hpp"
#include "Entities/runtime_entities.hpp"
#include "../gameplay_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fruityprime::gameplay {

using namespace detail;

std::size_t Session::player_index(std::uint8_t slot) const {
    const auto found = std::find_if(players_.begin(), players_.end(),
        [slot](const net::PlayerState& player) {
            return player.slot_index == slot;
        });
    if (found == players_.end()) {
        throw std::out_of_range("gameplay player slot is not active");
    }
    return static_cast<std::size_t>(found - players_.begin());
}

bool Session::has_player(std::uint8_t slot) const noexcept {
    return std::find_if(players_.begin(), players_.end(),
        [slot](const net::PlayerState& player) {
            return player.slot_index == slot;
        }) != players_.end();
}

std::uint32_t Session::respawn_ticks(std::uint8_t slot) const noexcept {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    return found == inputs_.end() ? 0u : found->respawn_ticks;
}

void Session::set_respawn_ticks(std::uint8_t slot,
                                std::uint32_t value) noexcept {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& input) { return input.slot == slot; });
    if (found != inputs_.end()) {
        found->respawn_ticks = value;
    }
}

net::Vec3 Session::spawn_position(std::size_t ordinal,
                                  std::uint8_t team) const {
    std::vector<net::Vec3> spawns;
    std::vector<net::Vec3> fallback_spawns;
    for (const auto& spawn : player_spawns_) {
        if (!spawn.active) {
            continue;
        }
        fallback_spawns.push_back(spawn.position);
        const auto entity = std::find_if(
            room_.entities().begin(), room_.entities().end(),
            [&spawn](const scene::EntityInstance& value) {
                return value.kind == scene::EntityKind::PlayerSpawn
                    && value.entity_id == spawn.entity_id;
            });
        const auto* data = entity == room_.entities().end()
            ? nullptr
            : std::get_if<scene::PlayerSpawnData>(&entity->typed_data);
        if (config_.team_mode && data != nullptr
            && data->team_index >= 0
            && static_cast<std::uint8_t>(data->team_index) != team) {
            continue;
        }
        spawns.push_back(spawn.position);
    }
    if (spawns.empty()) {
        spawns = std::move(fallback_spawns);
    }
    if (spawns.empty()) {
        return {0.0F, 0.0F, 0.0F};
    }
    return spawns[ordinal % spawns.size()];
}

Session::PlayerSpawnRuntime* Session::select_respawn_spawn(
    const net::PlayerState& player) {
    PlayerSpawnRuntime* selected = nullptr;
    std::array<PlayerSpawnRuntime*, 25> valid{};
    std::size_t valid_count = 0;
    PlayerSpawnRuntime* best_available = nullptr;
    float best_distance = 0.0F;
    std::size_t checked = 0;

    for (auto& candidate : player_spawns_) {
        if (checked >= valid.size()) {
            break;
        }
        ++checked;
        if (!candidate.active || candidate.cooldown != 0
            || (tick_count_ == 0 && candidate.availability)) {
            continue;
        }
        if (config_.mode == static_cast<std::uint8_t>(game::Mode::Capture)) {
            const auto entity = std::find_if(
                room_.entities().begin(), room_.entities().end(),
                [&candidate](const scene::EntityInstance& value) {
                    return value.kind == scene::EntityKind::PlayerSpawn
                        && value.entity_id == candidate.entity_id;
                });
            const auto* data = entity == room_.entities().end()
                ? nullptr
                : std::get_if<scene::PlayerSpawnData>(&entity->typed_data);
            if (data != nullptr && data->team_index >= 0
                && static_cast<std::uint8_t>(data->team_index) != player.team) {
                continue;
            }
        }

        float minimum_distance = 100.0F;
        for (const auto& other : players_) {
            if (other.health == 0
                || (other.flags & net::PlayerState::FlagSpectating) != 0) {
                continue;
            }
            minimum_distance = std::min(
                minimum_distance, distance_squared(candidate.position,
                                                  other.position));
        }
        if (minimum_distance >= 100.0F) {
            if (valid_count < valid.size()) {
                valid[valid_count++] = &candidate;
            }
        } else if (minimum_distance > best_distance) {
            best_distance = minimum_distance;
            best_available = &candidate;
        }
    }

    if (valid_count != 0) {
        selected = valid[static_cast<std::size_t>(
            tick_count_ % valid_count)];
    } else {
        selected = best_available;
    }
    if (selected == nullptr) {
        for (auto& fallback : player_spawns_) {
            if (fallback.active) {
                selected = &fallback;
                break;
            }
        }
    }
    if (selected != nullptr) {
        selected->cooldown = 4;
    }
    return selected;
}

std::size_t Session::add_player(std::uint8_t slot, std::uint8_t hunter) {
    if (players_.size() >= net::NetConfig::SlotCapacity) {
        throw std::length_error("gameplay session has eight player slots");
    }
    if (std::find_if(players_.begin(), players_.end(),
                     [slot](const net::PlayerState& player) {
                         return player.slot_index == slot;
                     }) != players_.end()) {
        throw std::invalid_argument("gameplay player slot is already active");
    }
    net::PlayerState player;
    player.slot_index = slot;
    player.flags = net::PlayerState::FlagActive
        | net::PlayerState::FlagSpawned;
    player.team = config_.team_mode
        ? static_cast<std::uint8_t>(slot & 1)
        : slot;
    player.position = spawn_position(players_.size(), player.team);
    player.facing = {0.0F, 0.0F, 1.0F};
    player.health = config_.max_health;
    player.current_weapon = 0;
    player.damage_beam = 0xff;
    player.points = 0;
    const std::size_t spawn_ordinal = players_.size();
    players_.push_back(player);
    RuntimeInput runtime;
    runtime.slot = slot;
    runtime.hunter = hunter;
    runtime.spawn_ordinal = spawn_ordinal;
    runtime.inventory.health_max = config_.max_health;
    runtime.inventory.available_weapons[0] = true;
    runtime.inventory.available_weapons[1] = true;
    // Multiplayer starts with the missile weapon and its native inventory
    // pool. Firing consumes the same metadata-defined cost used by the
    // managed BeamProjectileEntity spawn path.
    runtime.inventory.ammo[1] = 100;
    inputs_.push_back(runtime);
    players::PlayerEntity::SessionPlayerAdded(*this, slot, hunter);
    if (config_.mode == 14 && objectives_.prime_hunter < 0) {
        objectives_.prime_hunter = static_cast<std::int8_t>(slot);
    }
    return players_.size() - 1;
}

void Session::configure_story_hunter(std::uint8_t slot, net::Vec3 position,
                                     net::Vec3 facing,
                                     std::uint16_t health,
                                     std::uint16_t health_max,
                                     std::uint16_t health_threshold,
                                     std::uint32_t cartridge_weapon) {
    const std::size_t index = player_index(slot);
    const auto runtime = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (runtime == inputs_.end()) {
        throw std::out_of_range("story hunter slot is not active");
    }

    const auto effective_health_max = health_max == 0
        ? config_.max_health : health_max;
    runtime->inventory.health_max = effective_health_max;
    runtime->hunter_health_threshold = health_threshold;
    players_[index].position = position;
    players_[index].facing = facing;
    players_[index].health = std::min(health, effective_health_max);
    players_[index].flags |= net::PlayerState::FlagActive
        | net::PlayerState::FlagSpawned;

    // A story hunter with an authored weapon has only that weapon and never
    // consumes ammunition. This is the native equivalent of InitEnemyHunter;
    // the authored number is still the managed BeamType value.
    if (cartridge_weapon == 255) {
        return;
    }
    const auto native_weapon = metadata::native_weapon_slot_from_beam(
        static_cast<std::int32_t>(cartridge_weapon));
    if (native_weapon == 0xff
        || native_weapon >= runtime->inventory.available_weapons.size()) {
        return;
    }
    runtime->inventory.available_weapons.fill(false);
    runtime->inventory.available_weapons[native_weapon] = true;
    runtime->inventory.infinite_ammo = true;
    runtime->inventory.ammo.fill(0);
    runtime->inventory.ammo_max.fill(
        std::numeric_limits<std::uint16_t>::max());
    const auto& weapon = metadata::weapon_info(native_weapon);
    if (weapon.ammo_type < runtime->inventory.ammo.size()) {
        runtime->inventory.ammo[weapon.ammo_type] =
            std::numeric_limits<std::uint16_t>::max();
    }
    const std::uint8_t previous_weapon = players_[index].current_weapon;
    players_[index].current_weapon = native_weapon;
    players::PlayerEntity::SessionWeaponChanged(
        *this, slot, previous_weapon, native_weapon);
}

void Session::remove_player(std::uint8_t slot) {
    const std::size_t index = player_index(slot);
    players_.erase(players_.begin() + static_cast<std::ptrdiff_t>(index));
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
        [slot](const Projectile& projectile) {
            return projectile.owner_slot == slot;
        }), projectiles_.end());
    const auto input = std::find_if(inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (input != inputs_.end()) {
        inputs_.erase(input);
    }
    players::PlayerEntity::SessionPlayerRemoved(*this, slot);
    if (objectives_.prime_hunter == static_cast<std::int8_t>(slot)) {
        objectives_.prime_hunter = -1;
    }
    for (auto& node : objectives_.nodes) {
        if (node.captured_by_slot == slot) {
            node.captured_by_slot = 0xff;
            node.current_team = NeutralObjectiveTeam;
            node.occupying_team = NeutralObjectiveTeam;
            node.progress = 0.0F;
            node.score_timer = 0.0F;
        }
    }
    for (auto& flag : objectives_.flags) {
        if (flag.carrier_slot == slot) {
            flag.carrier_slot = 0xff;
            flag.at_base = false;
            flag.reset_timer = 0.0F;
        }
    }
}

void Session::set_player_hunter(std::uint8_t slot,
                                std::uint8_t hunter) noexcept {
    const auto input = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (input != inputs_.end()) {
        input->hunter = hunter;
    }
}

std::uint8_t Session::player_hunter(std::uint8_t slot) const noexcept {
    const auto input = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    return input == inputs_.end() ? 0xff : input->hunter;
}

void Session::place_player(std::uint8_t slot, net::Vec3 position,
                           net::Vec3 facing, bool alt_form) {
    auto& player = players_[player_index(slot)];
    player.position = position;
    player.facing = normalized_or(facing, {0.0F, 0.0F, 1.0F});
    player.speed = {0.0F, 0.0F, 0.0F};
    if (alt_form) {
        player.flags |= net::PlayerState::FlagAltForm;
    } else {
        player.flags &= static_cast<std::uint8_t>(
            ~net::PlayerState::FlagAltForm);
    }
}


void Session::rejoin_player(std::uint8_t slot) {
    net::PlayerState& player = players_[player_index(slot)];
    player.flags &= static_cast<std::uint8_t>(
        ~net::PlayerState::FlagSpectating);
    // Match points below zero are penalties and must survive a spectator
    // reset, while positive progress and the local kill/death counters start
    // over when the player returns to the match.
    player.points = std::min<std::int16_t>(0, player.points);
    player.kills = 0;
    player.deaths = 0;
}

void Session::set_match_mode(std::uint8_t mode,
                             std::uint16_t point_goal) noexcept {
    config_.mode = mode;
    config_.point_goal = point_goal;
    if (mode != 14) {
        objectives_.prime_hunter = -1;
        return;
    }
    if (objectives_.prime_hunter >= 0) {
        return;
    }
    const auto found = std::find_if(
        players_.begin(), players_.end(),
        [](const net::PlayerState& player) {
            return (player.flags & net::PlayerState::FlagActive) != 0;
        });
    if (found != players_.end()) {
        objectives_.prime_hunter = static_cast<std::int8_t>(
            found->slot_index);
    }
}

void Session::set_team_mode(bool enabled) noexcept {
    config_.team_mode = enabled;
    for (auto& player : players_) {
        player.team = enabled
            ? static_cast<std::uint8_t>(player.slot_index & 1)
            : player.slot_index;
    }
}

const net::PlayerState& Session::player(std::uint8_t slot) const {
    return players_[player_index(slot)];
}

net::PlayerState& Session::mutable_player(std::uint8_t slot) {
    return players_[player_index(slot)];
}

InventoryState& Session::inventory(std::uint8_t slot) {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        throw std::out_of_range("gameplay player slot is not active");
    }
    return found->inventory;
}

const players::Profile& Session::player_profile(std::uint8_t slot) const {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        throw std::out_of_range("gameplay player slot is not active");
    }
    return players::profile(found->hunter);
}

void Session::block_form_switch(std::uint8_t slot) noexcept {
    if (!has_player(slot)) {
        return;
    }
    inputs_[player_index(slot)].prevent_form_switch = true;
}

void Session::allow_form_switch(std::uint8_t slot) noexcept {
    if (!has_player(slot)) {
        return;
    }
    inputs_[player_index(slot)].prevent_form_switch = false;
}

bool Session::form_switch_blocked(std::uint8_t slot) const noexcept {
    if (!has_player(slot)) {
        return false;
    }
    return inputs_[player_index(slot)].prevent_form_switch;
}

void Session::on_halfturret_died(std::uint8_t slot) noexcept {
    set_halfturret(slot, false);
}

void Session::set_halfturret(std::uint8_t slot, bool active) noexcept {
    if (!has_player(slot)) {
        return;
    }
    auto& value = inputs_[player_index(slot)].halfturret;
    const bool was_active = value;
    value = active;
    if (active && !was_active && slot < players::PlayerEntity::SlotCapacity) {
        if (auto* owner = players::PlayerEntity::Players()[slot];
            owner != nullptr) {
            owner->Halfturret().initialize_from_owner();
        }
    }
}

bool Session::has_halfturret(std::uint8_t slot) const noexcept {
    if (!has_player(slot)) {
        return false;
    }
    return inputs_[player_index(slot)].halfturret;
}

void Session::save_status(std::uint8_t slot, game::StorySave& save,
                          bool fade_active) const {
    if (!has_player(slot)) {
        return;
    }
    const std::size_t index = player_index(slot);
    const net::PlayerState& state = players_[index];
    if (state.health == 0) {
        // A dead player writes nothing: the save must keep what they were
        // carrying when they were alive.
        return;
    }
    const RuntimeInput& runtime = inputs_[index];
    for (std::size_t i = 0; i < save.weapon_slots.size()
                            && i < runtime.inventory.available_weapons.size();
         ++i) {
        // The save holds the cartridge weapon ordinals; an unavailable slot
        // stays at -1 rather than being written as zero, which would read back
        // as the Power Beam.
        if (!runtime.inventory.available_weapons[i]) {
            continue;
        }
        save.weapon_slots[i] = static_cast<std::int32_t>(i);
    }
    for (std::size_t i = 0;
         i < save.ammo.size() && i < runtime.inventory.ammo.size(); ++i) {
        save.ammo[i] = static_cast<std::int32_t>(runtime.inventory.ammo[i]);
    }
    save.health = static_cast<std::int32_t>(state.health);
    if (!fade_active) {
        // No fade means the player walked out of the room under their own
        // steam, so the checkpoint they were holding is spent.
        save.checkpoint_room_id = -1;
        save.checkpoint_entity_id = -1;
    }
}

} // namespace fruityprime::gameplay

namespace fruityprime::players {

namespace {

[[nodiscard]] bool has_flag(formats::LoadFlags value,
                            formats::LoadFlags flag) noexcept {
    return (static_cast<std::uint8_t>(value)
            & static_cast<std::uint8_t>(flag)) != 0;
}

} // namespace

PlayerEntity::PlayerEntity(int slot_index, gameplay::Session& session)
    : session_(&session), slot_index_(slot_index),
      halfturret_(std::make_unique<runtime::HalfturretEntity>(
          0x80000000U | static_cast<std::uint32_t>(slot_index), *this)) {}

PlayerEntity::~PlayerEntity() = default;

void PlayerEntity::Construct(gameplay::Session& session,
                             const game::State* game_state) {
    bound_session_ = &session;
    bound_game_state_ = game_state;
    GeneratePlayerVolumes();
    for (int index = 0; index < SlotCapacity; ++index) {
        if (storage_[static_cast<std::size_t>(index)] == nullptr) {
            storage_[static_cast<std::size_t>(index)] =
                std::unique_ptr<PlayerEntity>(new PlayerEntity(index, session));
        } else {
            storage_[static_cast<std::size_t>(index)]->session_ = &session;
        }
        players_[static_cast<std::size_t>(index)] =
            storage_[static_cast<std::size_t>(index)].get();
    }
}

void PlayerEntity::Reset() noexcept {
    storage_ = {};
    players_ = {};
    bound_session_ = nullptr;
    bound_game_state_ = nullptr;
    player_count_ = 0;
    players_created_ = 0;
    main_player_index_ = 0;
}

PlayerEntity* PlayerEntity::Create(metadata::Hunter hunter, int recolor) {
    if (players_created_ >= max_players_ || players_created_ >= SlotCapacity
        || bound_session_ == nullptr) {
        return nullptr;
    }
    if (players_[0] == nullptr) {
        Construct(*bound_session_);
    }
    PlayerEntity* player = players_[static_cast<std::size_t>(players_created_)];
    ++players_created_;
    player->assign(hunter, recolor);
    return player;
}

void PlayerEntity::SessionPlayerAdded(gameplay::Session& session,
                                      std::uint8_t slot,
                                      std::uint8_t hunter) noexcept {
    if (bound_session_ != &session || slot >= SlotCapacity) {
        return;
    }
    auto* player = players_[slot];
    if (player == nullptr) {
        return;
    }
    player->assign(static_cast<metadata::Hunter>(hunter), 0);
    if ((session.player(slot).flags & net::PlayerState::FlagSpawned) != 0) {
        player->load_flags_ = static_cast<formats::LoadFlags>(
            static_cast<std::uint8_t>(player->load_flags_)
            | static_cast<std::uint8_t>(formats::LoadFlags::Spawned));
        player->runtime_state_.LoadFlags = player->load_flags_;
    }
    players_created_ = std::max(players_created_, static_cast<int>(slot) + 1);
    player_count_ = static_cast<int>(session.players().size());
}

void PlayerEntity::SessionPlayerRemoved(gameplay::Session& session,
                                        std::uint8_t slot) noexcept {
    if (bound_session_ != &session || slot >= SlotCapacity) {
        return;
    }
    if (players_[slot] != nullptr) {
        players_[slot]->load_flags_ = formats::LoadFlags::None;
    }
    player_count_ = static_cast<int>(session.players().size());
}

void PlayerEntity::SessionWeaponChanged(gameplay::Session& session,
                                        std::uint8_t slot,
                                        std::uint8_t previous_weapon,
                                        std::uint8_t current_weapon) noexcept {
    if (bound_session_ != &session || slot >= SlotCapacity) {
        return;
    }
    PlayerEntity* player = players_[slot];
    if (player == nullptr) {
        return;
    }
    player->runtime_state_.PreviousWeapon = static_cast<formats::BeamType>(
        metadata::beam_type_from_native_weapon_slot(previous_weapon));
    player->runtime_state_.CurrentWeapon = static_cast<formats::BeamType>(
        metadata::beam_type_from_native_weapon_slot(current_weapon));
    player->runtime_state_.WeaponSelection =
        player->runtime_state_.CurrentWeapon;
}

void PlayerEntity::SessionMovementChanged(gameplay::Session& session,
                                          std::uint8_t slot,
                                          net::Vec3 previous_position,
                                          net::Vec3 previous_speed) noexcept {
    if (bound_session_ != &session || slot >= SlotCapacity
        || players_[slot] == nullptr || !session.has_player(slot)) {
        return;
    }
    PlayerEntity& player = *players_[slot];
    const net::Vec3 speed = session.player(slot).speed;
    player.runtime_state_.PrevPosition = {
        previous_position.x, previous_position.y, previous_position.z};
    player.runtime_state_.PrevSpeed = {
        previous_speed.x, previous_speed.y, previous_speed.z};
    player.runtime_state_.Speed = {speed.x, speed.y, speed.z};
    player.runtime_state_.Acceleration = {
        speed.x - previous_speed.x,
        speed.y - previous_speed.y,
        speed.z - previous_speed.z};
}

void PlayerEntity::SessionPlayerTick(gameplay::Session& session,
                                     std::uint8_t slot) noexcept {
    if (bound_session_ != &session || slot >= SlotCapacity
        || players_[slot] == nullptr) {
        return;
    }
    auto& runtime = players_[slot]->runtime_state_;
    if (runtime.TimeSinceShot != std::numeric_limits<std::uint16_t>::max()) {
        ++runtime.TimeSinceShot;
    }
    runtime.RespawnTimer = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(session.respawn_ticks(slot), 0xffff));
}

void PlayerEntity::SessionShotFired(gameplay::Session& session,
                                    std::uint8_t slot) noexcept {
    if (bound_session_ == &session && slot < SlotCapacity
        && players_[slot] != nullptr) {
        players_[slot]->runtime_state_.TimeSinceShot = 0;
    }
}

void PlayerEntity::assign(metadata::Hunter hunter, int recolor) noexcept {
    hunter_ = hunter;
    recolor_ = recolor;
    load_flags_ = static_cast<formats::LoadFlags>(
        static_cast<std::uint8_t>(load_flags_)
        | static_cast<std::uint8_t>(formats::LoadFlags::SlotActive));
    load_flags_ = static_cast<formats::LoadFlags>(
        static_cast<std::uint8_t>(load_flags_)
        & ~static_cast<std::uint8_t>(formats::LoadFlags::Spawned));
    runtime_state_.Hunter = static_cast<formats::Hunter>(hunter);
    runtime_state_.SlotIndex = slot_index_;
    runtime_state_.LoadFlags = load_flags_;
    runtime_state_.Values = Values();
    if (has_live_state()) {
        runtime_state_.CurrentWeapon = CurrentWeapon();
        runtime_state_.PreviousWeapon = runtime_state_.CurrentWeapon;
        runtime_state_.WeaponSelection = runtime_state_.CurrentWeapon;
        runtime_state_.TeamIndex = State().team;
        runtime_state_.Team = Team();
    }
    ai_data_.reset();
    ai_data_.set_bot_level(bot_level_);
    camera_.reset();
    scan_state_.after_scan();
    sound_state_.stop_all_sfx();
}

bool PlayerEntity::has_live_state() const noexcept {
    return session_ != nullptr && session_->has_player(
        static_cast<std::uint8_t>(slot_index_));
}

void PlayerEntity::BotLevel(int value) noexcept {
    bot_level_ = std::clamp(value, 0, 2);
    ai_data_.set_bot_level(bot_level_);
}

runtime::HalfturretEntity& PlayerEntity::Halfturret() noexcept {
    return *halfturret_;
}

const runtime::HalfturretEntity& PlayerEntity::Halfturret() const noexcept {
    return *halfturret_;
}

void PlayerEntity::CreateHalfturret() {
    if (halfturret_ == nullptr) {
        halfturret_ = std::make_unique<runtime::HalfturretEntity>(
            0x80000000U | static_cast<std::uint32_t>(slot_index_), *this);
    }
}

bool PlayerEntity::HasHalfturret() const noexcept {
    return session_ != nullptr && session_->has_halfturret(
        static_cast<std::uint8_t>(slot_index_));
}

int PlayerEntity::MainPlayerIndex() noexcept { return main_player_index_; }
void PlayerEntity::MainPlayerIndex(int value) noexcept {
    main_player_index_ = value;
}
int PlayerEntity::PlayerCount() noexcept { return player_count_; }
void PlayerEntity::PlayerCount(int value) noexcept { player_count_ = value; }
int PlayerEntity::MaxPlayers() noexcept { return max_players_; }
void PlayerEntity::MaxPlayers(int value) noexcept { max_players_ = value; }
int PlayerEntity::PlayersCreated() noexcept { return players_created_; }

PlayerEntity* PlayerEntity::Main() noexcept {
    return main_player_index_ >= 0 && main_player_index_ < SlotCapacity
        ? players_[static_cast<std::size_t>(main_player_index_)] : nullptr;
}

std::span<PlayerEntity* const> PlayerEntity::Players() noexcept {
    return players_;
}

bool PlayerEntity::IsMainPlayer() const noexcept {
    return this == Main();
}

bool PlayerEntity::IsPrimeHunter() const noexcept {
    return bound_game_state_ != nullptr
        && slot_index_ == bound_game_state_->prime_hunter;
}

net::PlayerState& PlayerEntity::State() {
    if (session_ == nullptr || !session_->has_player(slot_index_)) {
        throw std::out_of_range("PlayerEntity slot is not active");
    }
    return session_->mutable_player(static_cast<std::uint8_t>(slot_index_));
}

const net::PlayerState& PlayerEntity::State() const {
    if (session_ == nullptr || !session_->has_player(slot_index_)) {
        throw std::out_of_range("PlayerEntity slot is not active");
    }
    return session_->player(static_cast<std::uint8_t>(slot_index_));
}

gameplay::InventoryState& PlayerEntity::Inventory() {
    return session_->inventory(static_cast<std::uint8_t>(slot_index_));
}

const gameplay::InventoryState& PlayerEntity::Inventory() const {
    return session_->inventory(static_cast<std::uint8_t>(slot_index_));
}

const entities::PlayerValues& PlayerEntity::Values() const noexcept {
    const auto index = std::min<std::size_t>(
        static_cast<std::size_t>(hunter_), metadata::PlayerValuesTable.size() - 1);
    return metadata::PlayerValuesTable[index];
}

int PlayerEntity::Health() const { return State().health; }
void PlayerEntity::Health(int value) {
    State().health = static_cast<std::uint16_t>(std::clamp(value, 0, 65535));
}
int PlayerEntity::HealthMax() const { return Inventory().health_max; }

AvailableArray& PlayerEntity::AvailableWeapons() {
    available_weapons_.bind(Inventory().available_weapons);
    return available_weapons_;
}

formats::BeamType PlayerEntity::CurrentWeapon() const {
    if (!has_live_state()) {
        return runtime_state_.CurrentWeapon;
    }
    return static_cast<formats::BeamType>(
        metadata::beam_type_from_native_weapon_slot(State().current_weapon));
}

formats::Team PlayerEntity::Team() const {
    if (!has_live_state()) {
        return runtime_state_.Team;
    }
    const int index = State().team;
    return (index & 1) == 0 ? formats::Team::Orange : formats::Team::Green;
}

void PlayerEntity::Team(formats::Team value) {
    runtime_state_.Team = value;
    if (value == formats::Team::None) {
        return;
    }
    const int parity = value == formats::Team::Green ? 1 : 0;
    int index = TeamIndex();
    if ((index & 1) != parity) {
        index = parity;
    }
    TeamIndex(index);
}

int PlayerEntity::TeamIndex() const {
    return has_live_state() ? State().team : runtime_state_.TeamIndex;
}

void PlayerEntity::TeamIndex(int value) {
    if (value < -1 || value > 255 || (has_live_state() && value < 0)) {
        throw std::out_of_range("TeamIndex");
    }
    if (has_live_state()) {
        State().team = static_cast<std::uint8_t>(value);
    }
    runtime_state_.TeamIndex = value;
    runtime_state_.Team = value < 0 ? formats::Team::None
        : (value & 1) == 0 ? formats::Team::Orange : formats::Team::Green;
}

formats::PlayerFlags1 PlayerEntity::Flags1() const noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(flags1_);
    constexpr auto AltForm = static_cast<std::uint32_t>(
        formats::PlayerFlags1::AltForm);
    if (has_live_state()) {
        if ((session_->player(static_cast<std::uint8_t>(slot_index_)).flags
             & net::PlayerState::FlagAltForm) != 0) {
            value |= AltForm;
        } else {
            value &= ~AltForm;
        }
    }
    return static_cast<formats::PlayerFlags1>(value);
}

formats::PlayerFlags2 PlayerEntity::Flags2() const noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(runtime_state_.Flags2);
    constexpr auto Halfturret = static_cast<std::uint32_t>(
        formats::PlayerFlags2::Halfturret);
    if (HasHalfturret()) {
        value |= Halfturret;
    } else {
        value &= ~Halfturret;
    }
    return static_cast<formats::PlayerFlags2>(value);
}

bool PlayerEntity::IsAltForm() const noexcept {
    return (static_cast<std::uint32_t>(Flags1())
            & static_cast<std::uint32_t>(formats::PlayerFlags1::AltForm)) != 0;
}
bool PlayerEntity::IsMorphing() const noexcept {
    return (static_cast<std::uint32_t>(flags1_)
            & static_cast<std::uint32_t>(formats::PlayerFlags1::Morphing)) != 0;
}
bool PlayerEntity::IsUnmorphing() const noexcept {
    return (static_cast<std::uint32_t>(flags1_)
            & static_cast<std::uint32_t>(formats::PlayerFlags1::Unmorphing)) != 0;
}
net::Vec3 PlayerEntity::Position() const { return State().position; }
net::Vec3 PlayerEntity::FacingVector() const { return State().facing; }
net::Vec3 PlayerEntity::Speed() const { return State().speed; }
void PlayerEntity::Speed(net::Vec3 value) {
    State().speed = value;
    runtime_state_.Speed = {value.x, value.y, value.z};
}

net::Vec3 PlayerEntity::Acceleration() const noexcept {
    const auto& value = runtime_state_.Acceleration;
    return {value.x, value.y, value.z};
}
void PlayerEntity::Acceleration(net::Vec3 value) noexcept {
    runtime_state_.Acceleration = {value.x, value.y, value.z};
}
net::Vec3 PlayerEntity::PrevSpeed() const noexcept {
    const auto& value = runtime_state_.PrevSpeed;
    return {value.x, value.y, value.z};
}
void PlayerEntity::PrevSpeed(net::Vec3 value) noexcept {
    runtime_state_.PrevSpeed = {value.x, value.y, value.z};
}
net::Vec3 PlayerEntity::PrevPosition() const noexcept {
    const auto& value = runtime_state_.PrevPosition;
    return {value.x, value.y, value.z};
}
void PlayerEntity::PrevPosition(net::Vec3 value) noexcept {
    runtime_state_.PrevPosition = {value.x, value.y, value.z};
}
net::Vec3 PlayerEntity::IdlePosition() const noexcept {
    const auto& value = runtime_state_.IdlePosition;
    return {value.x, value.y, value.z};
}
std::uint16_t PlayerEntity::TimeSinceShot() const noexcept {
    return runtime_state_.TimeSinceShot;
}
void PlayerEntity::TimeSinceShot(std::uint16_t value) noexcept {
    runtime_state_.TimeSinceShot = value;
}
std::uint16_t PlayerEntity::RespawnTimer() const noexcept {
    const std::uint32_t value = session_ == nullptr
        ? runtime_state_.RespawnTimer
        : session_->respawn_ticks(static_cast<std::uint8_t>(slot_index_));
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0xffff));
}
void PlayerEntity::RespawnTimer(std::uint16_t value) noexcept {
    runtime_state_.RespawnTimer = value;
    if (session_ != nullptr) {
        session_->set_respawn_ticks(
            static_cast<std::uint8_t>(slot_index_), value);
    }
}
bool PlayerEntity::DoubleDamage() const noexcept {
    if (!has_live_state()) {
        return runtime_state_.DoubleDamage;
    }
    return session_->inventory(static_cast<std::uint8_t>(slot_index_))
        .double_damage_ticks > 0;
}

void PlayerEntity::Spawn(net::Vec3 position, net::Vec3 facing, net::Vec3 up,
                         bool respawn) {
    if (respawn) {
        position.y += 1.0F;
    }
    up_vector_ = up;
    session_->place_player(static_cast<std::uint8_t>(slot_index_), position,
                           facing, false);
    load_flags_ = static_cast<formats::LoadFlags>(
        static_cast<std::uint8_t>(load_flags_)
        | static_cast<std::uint8_t>(formats::LoadFlags::Spawned));
    runtime_state_.LoadFlags = load_flags_;
    runtime_state_.PrevPosition = {position.x, position.y, position.z};
    runtime_state_.IdlePosition = runtime_state_.PrevPosition;
    runtime_state_.Speed = {};
    runtime_state_.Acceleration = {};
    const float horizontal = std::sqrt(
        facing.x * facing.x + facing.z * facing.z);
    runtime_state_.Field70 = facing.x / horizontal;
    runtime_state_.Field74 = facing.z / horizontal;
    flags1_ = static_cast<formats::PlayerFlags1>(
        static_cast<std::uint32_t>(formats::PlayerFlags1::Standing)
        | static_cast<std::uint32_t>(formats::PlayerFlags1::StandingPrevious)
        | static_cast<std::uint32_t>(formats::PlayerFlags1::CanTouchBoost));
    runtime_state_.Flags1 = flags1_;
    runtime_state_.Flags2 = formats::PlayerFlags2::NoShotsFired;
    runtime_state_.TimeSinceShot = 255;
    runtime_state_.RespawnTimer = 0;
    runtime_state_.CurAlpha = 1.0F;
    runtime_state_.Volume = statics_.player_volume(
        static_cast<std::size_t>(hunter_), PlayerStatics::Volume::PickupLow);
    runtime_state_.Volume.SpherePosition =
        runtime_state_.Volume.SpherePosition
        + formats::Vector3{position.x, position.y, position.z};
    camera_.reset();
    camera_.info().position = position;
    camera_.info().target = {
        position.x + facing.x, position.y + facing.y, position.z + facing.z};
    camera_.info().up = up;
    camera_.switch_camera(CameraType::First, facing);
    camera_.update(State(), session_->player_profile(
        static_cast<std::uint8_t>(slot_index_)), 0.0F);
}

void PlayerEntity::Teleport(net::Vec3 position, net::Vec3 facing) {
    Reposition(position, facing);
}

void PlayerEntity::Reposition(net::Vec3 position, net::Vec3 facing) {
    session_->place_player(static_cast<std::uint8_t>(slot_index_), position,
                           facing, IsAltForm());
}

void PlayerEntity::Reposition(net::Vec3 offset) {
    const auto state = State();
    Reposition({state.position.x + offset.x, state.position.y + offset.y,
                state.position.z + offset.z}, state.facing);
    if (HasHalfturret()) {
        halfturret_->reposition(offset);
        halfturret_->reset_grounded_state();
    }
}

void PlayerEntity::BlockFormSwitch() {
    session_->block_form_switch(static_cast<std::uint8_t>(slot_index_));
}

void PlayerEntity::SetCombatVisor() noexcept {
    if (bound_game_state_ == nullptr || bound_game_state_->single_player) {
        scan_state_.set_combat_visor();
    }
}

void PlayerEntity::ResetCombatVisor() noexcept {
    if (bound_game_state_ == nullptr || bound_game_state_->single_player) {
        scan_state_.reset_combat_visor();
    }
}

void PlayerEntity::StartFlagCarrySfx() noexcept {
    sound_state_.start_flag_carry_sfx();
}

void PlayerEntity::StopFlagCarrySfx() noexcept {
    sound_state_.stop_flag_carry_sfx();
}

void PlayerEntity::StopAllSfx() noexcept {
    sound_state_.stop_all_sfx();
}

void PlayerEntity::StopTimedSfx() noexcept {
    sound_state_.stop_timed_sfx(sfx_mute_state_);
}

void PlayerEntity::RestartTimedSfx(bool force) noexcept {
    sound_state_.restart_timed_sfx(sfx_mute_state_, force);
}

void PlayerEntity::StopLongSfx() noexcept {
    sound_state_.stop_long_sfx(sfx_mute_state_);
}

void PlayerEntity::RestartLongSfx(bool force) noexcept {
    sound_state_.restart_long_sfx(sfx_mute_state_, force);
}

PlayerSoundState::TimedSoundUpdate PlayerEntity::UpdateTimedSounds(
    float frame_seconds, bool camera_blocks_input) noexcept {
    return sound_state_.update_timed_sounds(
        frame_seconds, sfx_mute_state_, camera_blocks_input);
}

void PlayerEntity::GainHealth(std::uint32_t health) {
    GainHealth(static_cast<std::int32_t>(health));
}

void PlayerEntity::GainHealth(std::int32_t health) {
    const bool has_halfturret = HasHalfturret();
    const std::int32_t turret_health = has_halfturret
        ? Halfturret().health() : 0;
    const auto result = PlayerProcess::gain_health(
        Health(), HealthMax(), health, has_halfturret, turret_health);
    Health(result.health);
    if (has_halfturret) {
        Halfturret().set_health(result.halfturret_health);
    }
}

void PlayerEntity::ExitAltForm() {
    if (HasHalfturret()) {
        const int reclaimed = std::max(0, Halfturret().health());
        runtime_state_.Flags2 = static_cast<formats::PlayerFlags2>(
            static_cast<std::uint32_t>(runtime_state_.Flags2)
            & ~static_cast<std::uint32_t>(formats::PlayerFlags2::Halfturret));
        Health(std::min(HealthMax(), Health() + reclaimed));
        static_cast<void>(Halfturret().die());
    }
    flags1_ = static_cast<formats::PlayerFlags1>(
        (static_cast<std::uint32_t>(flags1_)
         & ~static_cast<std::uint32_t>(formats::PlayerFlags1::Morphing)
         & ~static_cast<std::uint32_t>(formats::PlayerFlags1::AltForm))
        | static_cast<std::uint32_t>(formats::PlayerFlags1::Unmorphing));
    runtime_state_.Flags1 = flags1_;
    runtime_state_.Flags2 = static_cast<formats::PlayerFlags2>(
        static_cast<std::uint32_t>(runtime_state_.Flags2)
        & ~static_cast<std::uint32_t>(formats::PlayerFlags2::AltAttack));
    if (has_live_state()) {
        State().flags &= static_cast<std::uint8_t>(
            ~net::PlayerState::FlagAltForm);
    }
    UpdateZoom(false);
}

void PlayerEntity::OnHalfturretDied() {
    runtime_state_.Flags2 = static_cast<formats::PlayerFlags2>(
        static_cast<std::uint32_t>(runtime_state_.Flags2)
        & ~static_cast<std::uint32_t>(formats::PlayerFlags2::Halfturret));
    session_->on_halfturret_died(static_cast<std::uint8_t>(slot_index_));
}

bool PlayerEntity::GetTargetable() const {
    return has_live_state() && Health() > 0;
}

int PlayerEntity::GetScanId(bool alternate) const noexcept {
    const auto hunter = std::min<std::size_t>(
        static_cast<std::size_t>(hunter_), PlayerScan::ScanIds.size() - 1);
    const std::size_t form = IsAltForm() ? 1 : 0;
    return PlayerScan::ScanIds[hunter][form + (alternate ? 2 : 0)];
}

bool PlayerEntity::ScanVisible() const {
    return GetTargetable() && !IsMainPlayer();
}

void PlayerEntity::UpdateZoom(bool zoom) {
    if (zoom) {
        State().flags |= net::PlayerState::FlagZoomed;
    } else {
        State().flags &= static_cast<std::uint8_t>(
            ~net::PlayerState::FlagZoomed);
    }
}

void PlayerEntity::TakeDamage(std::int32_t damage, formats::DamageFlags flags,
                              const net::Vec3* direction,
                              PlayerEntity* source) {
    TakeDamage(damage > 0 ? static_cast<std::uint32_t>(damage) : 0U,
               flags, direction, source);
}

void PlayerEntity::TakeDamage(std::uint32_t damage, formats::DamageFlags flags,
                              const net::Vec3* direction,
                              PlayerEntity* source) {
    auto& state = State();
    const auto has_damage_flag = [flags](formats::DamageFlags flag) {
        return (static_cast<std::int32_t>(flags)
                & static_cast<std::int32_t>(flag)) != 0;
    };
    if (HasHalfturret() && source != nullptr) {
        halfturret_->on_take_damage(source, damage);
    }
    if (HasHalfturret() && has_damage_flag(formats::DamageFlags::Halfturret)) {
        std::uint32_t turret_damage = state.health > halfturret_->health()
            ? damage - damage / 2 : damage / 2;
        if (turret_damage >= static_cast<std::uint32_t>(
                                 std::max(0, halfturret_->health()))) {
            static_cast<void>(halfturret_->die());
        } else {
            halfturret_->set_health(
                halfturret_->health() - static_cast<std::int32_t>(
                    turret_damage));
        }
        halfturret_->set_time_since_damage(0);
        damage -= turret_damage;
        if (state.health <= damage) {
            damage = state.health > 0 ? state.health - 1 : 0;
        }
    }
    const auto applied = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        damage, state.health));
    state.health = static_cast<std::uint16_t>(state.health - applied);
    state.damage_flags = static_cast<std::uint8_t>(flags);
    state.attacker_slot = source == nullptr
        ? 0xff : static_cast<std::uint8_t>(source->SlotIndex());
    if (direction != nullptr) {
        state.hit_direction = *direction;
    }
    ++state.damage_sequence;
}

void PlayerEntity::SaveStatus(game::StorySave& save, bool fade_active) const {
    session_->save_status(static_cast<std::uint8_t>(slot_index_), save,
                          fade_active);
}

void PlayerEntity::ResetReferences() noexcept {
    runtime_state_.EnemySpawner = nullptr;
    runtime_state_.AttachedEnemy = nullptr;
    runtime_state_.MorphCamera = nullptr;
    runtime_state_.OctolithFlag = nullptr;
    runtime_state_.BurnedBy = nullptr;
    runtime_state_.ShockCoilTarget = nullptr;
}

void PlayerEntity::WeaponNameTable(std::vector<strings::TableEntry> entries) {
    weapon_name_table_ = std::move(entries);
}

void PlayerEntity::LoadWeaponNames() noexcept {
    try {
        for (std::size_t index = 0; index < weapon_names_.size(); ++index) {
            weapon_names_[index] = strings::get_message(
                weapon_name_table_, 'W', static_cast<std::uint32_t>(index + 1));
        }
        for (std::size_t index = 0; index < hunter_names_.size(); ++index) {
            hunter_names_[index] = strings::get_message(
                weapon_name_table_, 'H', static_cast<std::uint32_t>(index + 1));
        }
        // Guardian has no authored alt-attack name, exactly as in C#.
        for (std::size_t index = 0; index + 1 < alt_attack_names_.size(); ++index) {
            alt_attack_names_[index] = strings::get_message(
                weapon_name_table_, 'A', static_cast<std::uint32_t>(index + 1));
        }
        alt_attack_names_.back().clear();
    } catch (...) {
        weapon_names_ = {};
        hunter_names_ = {};
        alt_attack_names_ = {};
    }
}

const std::array<std::string, 9>& PlayerEntity::WeaponNames() noexcept {
    return weapon_names_;
}
const std::array<std::string, 8>& PlayerEntity::HunterNames() noexcept {
    return hunter_names_;
}
const std::array<std::string, 8>& PlayerEntity::AltAttackNames() noexcept {
    return alt_attack_names_;
}

void PlayerEntity::GeneratePlayerVolumes() noexcept {
    statics_.generate_player_volumes();
}

const formats::CollisionVolume& PlayerEntity::PlayerVolume(
    std::size_t hunter, PlayerStatics::Volume which) noexcept {
    return statics_.player_volume(hunter, which);
}

} // namespace fruityprime::players
