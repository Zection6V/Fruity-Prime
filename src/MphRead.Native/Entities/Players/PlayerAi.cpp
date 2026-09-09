// Native counterpart of src/MphRead/Entities/Players/PlayerAi.cs.
#include "Metadata/player_values.hpp"
#include "PlayerAi.hpp"

#include "Metadata/metadata.hpp"
#include "Entities/Players/ai_dispatch.generated.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fruityprime::players {
namespace {

[[nodiscard]] float distance_squared(net::Vec3 left,
                                     net::Vec3 right) noexcept {
    const net::Vec3 delta{
        left.x - right.x, left.y - right.y, left.z - right.z};
    return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

[[nodiscard]] bool active_target(const net::PlayerState& player) noexcept {
    return player.health > 0
        && (player.flags & net::PlayerState::FlagActive) != 0
        && (player.flags & net::PlayerState::FlagSpawned) != 0
        && (player.flags & net::PlayerState::FlagSpectating) == 0;
}

void add_button(gameplay::Input& input, net::IntentButtons button) noexcept {
    input.buttons = static_cast<net::IntentButtons>(
        static_cast<std::uint32_t>(input.buttons)
        | static_cast<std::uint32_t>(button));
}

[[nodiscard]] std::uint8_t choose_weapon(
    const gameplay::InventoryState& inventory, float distance,
    int level) noexcept {
    if (level >= 1 && distance >= 9.0F
        && inventory.available_weapons[1]
        && inventory.ammo[1] >= metadata::weapon_info(1).ammo_cost) {
        return 1;
    }
    if (level >= 2 && distance >= 16.0F
        && inventory.available_weapons[4]
        && inventory.ammo[0] >= metadata::weapon_info(4).ammo_cost) {
        return 4;
    }
    return 0;
}

[[nodiscard]] bool input_button(gameplay::Input input,
                                AiButtonId id) noexcept {
    const auto bits = static_cast<std::uint32_t>(input.buttons);
    const auto has = [bits](net::IntentButtons button) {
        return (bits & static_cast<std::uint32_t>(button)) != 0;
    };
    switch (id) {
    case AiButtonId::Up:
        return has(net::IntentButtons::MoveUp);
    case AiButtonId::Down:
        return has(net::IntentButtons::MoveDown);
    case AiButtonId::Left:
        return has(net::IntentButtons::MoveLeft);
    case AiButtonId::Right:
        return has(net::IntentButtons::MoveRight);
    case AiButtonId::Jump:
        return has(net::IntentButtons::Jump);
    case AiButtonId::Morph:
        return has(net::IntentButtons::Morph);
    case AiButtonId::Shoot:
        return has(net::IntentButtons::Shoot);
    case AiButtonId::AltAttack:
        return has(net::IntentButtons::AltAttack);
    case AiButtonId::Boost:
        return has(net::IntentButtons::Boost);
    case AiButtonId::Zoom:
        return has(net::IntentButtons::Zoom);
    case AiButtonId::NextWeapon:
        return has(net::IntentButtons::NextWeapon);
    case AiButtonId::PrevWeapon:
        return has(net::IntentButtons::PrevWeapon);
    case AiButtonId::Count:
        break;
    }
    return false;
}

[[nodiscard]] net::Vec3 subtract(net::Vec3 left,
                                 net::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float dot(net::Vec3 left, net::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length_squared(net::Vec3 value) noexcept {
    return dot(value, value);
}

} // namespace

bool AiAggroEntry::matches(std::uint8_t a2, std::uint8_t a3,
                            std::uint8_t a4, std::uint8_t first,
                            std::uint8_t second) const noexcept {
    return (a2 == 7 || var_a2 == a2)
        && (a3 == 7 || var_a3 == a3)
        && (a4 == 7 || var_a4 == a4)
        && (a3 != 2 || player1 == first)
        && (a4 != 2 || player2 == second);
}

void PlayerAiData::reset() noexcept {
    personality_ = nullptr;
    flags2_ = AiFlags2::None;
    flags3_ = AiFlags3::None;
    flags4_ = AiFlags4::None;
    buttons_.clear();
    for (auto& entry : player_aggro_) {
        entry.clear();
    }
    player_aggro_count_ = 0;
    for (auto& context : execution_tree_) {
        context.clear();
    }
    for (auto& row : visibility_) {
        row.fill(false);
    }
    slot_hits_.fill(0);
    slot_damage_.fill(0);
    damage_from_halfturret_ = 0;
    field118_ = 0;
    target_slot_ = 0xff;
    aggro_score_ = 0;
    bot_level_ = 0;
    weapon1_ = 0;
    weapon2_ = 0;
    find_weapon_index_ = 0;
    frame_count_ = 0;
    unknown_func3_calls_ = 0;
}

void PlayerAiData::initialize(const ai::Personality* personality) noexcept {
    reset();
    personality_ = personality;
    if (personality_ != nullptr && personality_->root != nullptr) {
        update_execution_path(nullptr, 0, personality_->root.get(),
                              0);
    }
}

void PlayerAiData::set_bot_level(int level) noexcept {
    bot_level_ = static_cast<std::uint8_t>(std::clamp(level, 0, 2));
}

void PlayerAiData::process_input(const gameplay::Input& input) noexcept {
    flags3_ |= AiFlags3::NoInput;
    constexpr auto count = static_cast<std::size_t>(AiButtonId::Count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = static_cast<AiButtonId>(i);
        auto& button = buttons_[id];
        if (input_button(input, id)) {
            flags3_ &= ~AiFlags3::NoInput;
            button.frames_up = 0;
            if (button.frames_down < 6000) {
                ++button.frames_down;
            }
            button.is_down = true;
        } else {
            button.frames_down = 0;
            if (button.frames_up < 6000) {
                ++button.frames_up;
            }
            button.is_down = false;
        }
    }
    if (input.weapon_select != 0xff) {
        weapon2_ = input.weapon_select;
        find_weapon_index_ = input.weapon_select;
    }
}

void PlayerAiData::notify_damage(std::uint8_t source_slot,
                                 std::uint32_t damage) noexcept {
    if (source_slot >= net::NetConfig::SlotCapacity) {
        return;
    }
    ++slot_hits_[source_slot];
    slot_damage_[source_slot] += damage;
    add_aggro(6, 2, 1, source_slot, 0xff, damage, 30, 10, 2);
}

void PlayerAiData::update_visibility(
    const gameplay::Session& session) noexcept {
    for (auto& row : visibility_) {
        row.fill(false);
    }
    // Session currently exposes player transforms but not its collision
    // query.  The managed code asks CollisionDetection for this same matrix.
    // Until that query is shared, active players are conservatively visible;
    // this preserves target/aggro behavior and never invents a wall hit.
    for (const auto& first : session.players()) {
        if (!active_target(first)
            || first.slot_index >= net::NetConfig::SlotCapacity) {
            continue;
        }
        for (const auto& second : session.players()) {
            if (first.slot_index == second.slot_index
                || !active_target(second)
                || second.slot_index >= net::NetConfig::SlotCapacity) {
                continue;
            }
            const float distance = distance_squared(
                first.position, second.position);
            visibility_[first.slot_index][second.slot_index] =
                std::isfinite(distance);
        }
    }
}

void PlayerAiData::update_aggro_expiration() noexcept {
    std::size_t index = 0;
    while (index < player_aggro_count_) {
        auto& entry = player_aggro_[index];
        if (entry.staleness < std::numeric_limits<std::uint16_t>::max()) {
            ++entry.staleness;
        }
        const std::uint32_t expiry =
            static_cast<std::uint32_t>(entry.expiration) * 2u;
        if (entry.staleness > expiry) {
            if (index + 1 < player_aggro_count_) {
                entry = player_aggro_[player_aggro_count_ - 1];
            }
            player_aggro_[player_aggro_count_ - 1].clear();
            --player_aggro_count_;
        } else {
            ++index;
        }
    }
}

void PlayerAiData::add_aggro(std::uint8_t a2, std::uint8_t a3,
                             std::uint8_t a4, std::uint8_t player1,
                             std::uint8_t player2, std::uint32_t value,
                             std::uint32_t expiration,
                             std::uint32_t priority,
                             std::uint8_t mode) noexcept {
    const auto find_entry = [&]() -> AiAggroEntry* {
        for (std::size_t i = 0; i < player_aggro_count_; ++i) {
            auto& entry = player_aggro_[i];
            if (entry.var_a10 != 1
                && entry.matches(a2, a3, a4, player1, player2)) {
                return &entry;
            }
        }
        return nullptr;
    };

    if (mode == 2) {
        if (auto* entry = find_entry(); entry != nullptr) {
            entry->expiration = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(54000u,
                                        entry->expiration + expiration));
            entry->var_a9 = static_cast<std::uint8_t>(
                std::min<std::uint32_t>(15u,
                                        std::max<std::uint32_t>(
                                            entry->var_a9, priority)));
            entry->var_a7 = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(4000u,
                                        entry->var_a7 + value));
            return;
        }
    } else if (mode == 3) {
        if (auto* entry = find_entry(); entry != nullptr) {
            entry->var_a2 = static_cast<std::uint8_t>(a2 & 0xf);
            entry->var_a9 = static_cast<std::uint8_t>(priority & 0xf);
            entry->var_a3 = static_cast<std::uint8_t>(a3 & 0xf);
            entry->var_a4 = static_cast<std::uint8_t>(a4 & 0xf);
            entry->var_a10 = 3;
            entry->var_a7 = static_cast<std::uint16_t>(value & 0xf);
            entry->staleness = 0;
            entry->expiration = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(expiration, 54000u));
            entry->player1 = player1;
            entry->player2 = player2;
            return;
        }
    }

    std::size_t index = player_aggro_count_;
    if (index >= MaxAggroEntries) {
        std::uint32_t minimum = priority;
        index = MaxAggroEntries;
        for (std::size_t i = 0; i < MaxAggroEntries; ++i) {
            if (player_aggro_[i].var_a9 < minimum) {
                minimum = player_aggro_[i].var_a9;
                index = i;
            }
        }
    } else {
        ++player_aggro_count_;
    }
    if (index >= MaxAggroEntries) {
        return;
    }
    auto& entry = player_aggro_[index];
    entry.var_a2 = static_cast<std::uint8_t>(a2 & 0xf);
    entry.var_a9 = static_cast<std::uint8_t>(priority & 0xf);
    entry.var_a3 = static_cast<std::uint8_t>(a3 & 0xf);
    entry.var_a4 = static_cast<std::uint8_t>(a4 & 0xf);
    entry.var_a10 = static_cast<std::uint8_t>(mode & 0xf);
    entry.var_a7 = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(value & 0xffffu, 4000u));
    entry.staleness = 0;
    entry.expiration = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(expiration, 54000u));
    entry.player1 = player1;
    entry.player2 = player2;
}

std::uint32_t PlayerAiData::aggro_priority(
    std::uint8_t a2, std::uint8_t a3, std::uint8_t a4,
    std::uint8_t player1, std::uint8_t player2) const noexcept {
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < player_aggro_count_; ++i) {
        const auto& entry = player_aggro_[i];
        if ((a2 == entry.var_a2 || a2 == 7)
            && (a3 == entry.var_a3 || a3 == 7)
            && (a4 == entry.var_a4 || a4 == 7)
            && (player1 == entry.player1 || a3 != 2)
            && (player2 == entry.player2 || a4 != 2)) {
            result += entry.var_a7;
        }
    }
    return result;
}

void PlayerAiData::update_aggro(const gameplay::Session& session,
                                std::uint8_t bot_slot) noexcept {
    update_aggro_expiration();
    target_slot_ = 0xff;
    aggro_score_ = 0;
    if (!session.has_player(bot_slot)) {
        flags2_ &= ~AiFlags2::TargetPlayer;
        return;
    }
    const auto& bot = session.player(bot_slot);
    float best_distance = std::numeric_limits<float>::max();
    std::uint32_t best_priority = 0;
    const net::PlayerState* target = nullptr;
    for (const auto& candidate : session.players()) {
        if (candidate.slot_index == bot_slot
            || !active_target(candidate)
            || candidate.slot_index >= net::NetConfig::SlotCapacity
            || (session.team_mode() && candidate.team == bot.team)
            || !visibility_[bot_slot][candidate.slot_index]) {
            continue;
        }
        add_aggro(6, 1, 2, 0xff, candidate.slot_index,
                  1, 30, 10, 3);
        const float current_distance = distance_squared(
            bot.position, candidate.position);
        const std::uint32_t priority = aggro_priority(
            6, 1, 2, 0xff, candidate.slot_index);
        if (target == nullptr || priority > best_priority
            || (priority == best_priority
                && current_distance < best_distance)) {
            target = &candidate;
            best_priority = priority;
            best_distance = current_distance;
        }
    }
    if (target == nullptr) {
        flags2_ &= ~AiFlags2::TargetPlayer;
        return;
    }
    target_slot_ = target->slot_index;
    aggro_score_ = best_priority;
    flags2_ |= AiFlags2::TargetPlayer;
    if (best_distance > 0.0F && std::isfinite(best_distance)) {
        const auto delta = subtract(target->position, bot.position);
        const auto facing_length = std::sqrt(length_squared(bot.facing));
        const auto delta_length = std::sqrt(best_distance);
        if (facing_length > 0.0F && delta_length > 0.0F) {
            const float facing_dot = dot(bot.facing, delta)
                / (facing_length * delta_length);
            if (facing_dot >= 255.0F / 256.0F) {
                flags4_ |= AiFlags4::Bit0;
            } else {
                flags4_ &= ~AiFlags4::Bit0;
            }
        }
    }
}


// ---- PlayerAi's vector functions -------------------------------------
//
// Eight of them, selected by ExecuteVectorFunc.  Three read the bot's own
// aim vectors, which is why those are handed in rather than recomputed: the
// renderer, the shot path and these all have to agree on where the muzzle is.

net::Vec3 PlayerAiData::func_213A470(const gameplay::Session& session,
                                     std::uint8_t bot_slot) const noexcept {
    // The target's position, relative to the bot.
    if (target_slot_ == 0xff || !session.has_player(target_slot_)
        || !session.has_player(bot_slot)) {
        return {};
    }
    const auto& target = session.player(target_slot_);
    const auto& bot = session.player(bot_slot);
    return {target.position.x - bot.position.x,
            target.position.y - bot.position.y,
            target.position.z - bot.position.z};
}

net::Vec3 PlayerAiData::func_213A458(const gameplay::Session& session,
                                     std::uint8_t bot_slot) const noexcept {
    // _player._facingVector.
    if (!session.has_player(bot_slot)) {
        return {0.0F, 0.0F, 1.0F};
    }
    return session.player(bot_slot).facing;
}

net::Vec3 PlayerAiData::func_213A3DC(
    const gameplay::Session& session,
    const PlayerProcess::AimVectors& aim) const noexcept {
    // From the muzzle to the target's centre of mass, which sits lower while
    // the target is a ball.
    if (target_slot_ == 0xff || !session.has_player(target_slot_)) {
        return {};
    }
    const auto& target = session.player(target_slot_);
    const bool alt = (target.flags & net::PlayerState::FlagAltForm) != 0;
    const auto& values = metadata::PlayerValuesTable[
        std::min<std::size_t>(static_cast<std::size_t>(
                                  session.player_profile(target_slot_).hunter),
                              metadata::HunterCount - 1)];
    const float lift = alt
        ? static_cast<float>(values.AltColYPos) / 4096.0F : 0.5F;
    return {target.position.x - aim.muzzle_pos.x,
            target.position.y + lift - aim.muzzle_pos.y,
            target.position.z - aim.muzzle_pos.z};
}

net::Vec3 PlayerAiData::func_213A3C0(
    const PlayerProcess::AimVectors& aim) const noexcept {
    // From the muzzle to where the bot is aiming.
    return {aim.aim_position.x - aim.muzzle_pos.x,
            aim.aim_position.y - aim.muzzle_pos.y,
            aim.aim_position.z - aim.muzzle_pos.z};
}

net::Vec3 PlayerAiData::func_213A3A8(
    const gameplay::Session& session) const noexcept {
    // Which way the target is facing.
    if (target_slot_ == 0xff || !session.has_player(target_slot_)) {
        return {0.0F, 0.0F, 1.0F};
    }
    return session.player(target_slot_).facing;
}

net::Vec3 PlayerAiData::func_213A37C(const gameplay::Session& session,
                                     std::uint8_t bot_slot) const noexcept {
    // Towards the node the bot is heading for.  Node navigation is not
    // ported yet, so this has nothing to point at and says so rather than
    // pointing somewhere arbitrary.
    static_cast<void>(session);
    static_cast<void>(bot_slot);
    return {};
}

net::Vec3 PlayerAiData::func_213A35C(net::Vec3 camera_facing) const noexcept {
    return camera_facing;
}

net::Vec3 PlayerAiData::func_213A31C(const gameplay::Session& session,
                                     std::uint8_t bot_slot) const noexcept {
    // Towards the entity in reference slot 26, which the managed code looks
    // up first.  The reference table is not populated by this head yet.
    static_cast<void>(session);
    static_cast<void>(bot_slot);
    return {};
}

net::Vec3 PlayerAiData::execute_vector_func(
    const gameplay::Session& session, std::uint8_t bot_slot,
    const PlayerProcess::AimVectors& aim, net::Vec3 camera_facing, int index,
    bool clear_y, bool normalize) const noexcept {
    net::Vec3 result{};
    switch (index) {
    case 0: result = func_213A470(session, bot_slot); break;
    case 1: result = func_213A458(session, bot_slot); break;
    case 2: result = func_213A3DC(session, aim); break;
    case 3: result = func_213A3C0(aim); break;
    case 4: result = func_213A3A8(session); break;
    case 5: result = func_213A37C(session, bot_slot); break;
    case 6: result = func_213A35C(camera_facing); break;
    case 7: result = func_213A31C(session, bot_slot); break;
    default: return {};
    }
    if (clear_y) {
        result.y = 0.0F;
    }
    if (normalize) {
        const float length_squared = result.x * result.x
            + result.y * result.y + result.z * result.z;
        if (length_squared <= 0.0F || !std::isfinite(length_squared)) {
            // The managed code falls back to the X axis rather than to zero,
            // so a bot with nothing to aim at still has a direction.
            return {1.0F, 0.0F, 0.0F};
        }
        const float inverse = 1.0F / std::sqrt(length_squared);
        result = {result.x * inverse, result.y * inverse,
                  result.z * inverse};
    }
    return result;
}

std::uint8_t PlayerAiData::get_beam_type(int weapon) noexcept {
    // The bot numbers Missile 1 and Volt Driver 2; the cartridge has them
    // the other way round.  Every other beam is its own number.
    switch (weapon) {
    case 1: return 1;   // BeamType.Missile
    case 2: return 2;   // BeamType.VoltDriver
    default: return static_cast<std::uint8_t>(weapon);
    }
}

bool PlayerAiData::check_beam(const gameplay::Session& session,
                              std::uint8_t bot_slot,
                              std::uint8_t beam) const noexcept {
    if (!session.has_player(bot_slot) || beam >= metadata::WeaponCount) {
        return false;
    }
    const auto& info = metadata::weapon_info(beam);
    const auto& inventory = session.inventory(bot_slot);
    const auto type = std::min<std::size_t>(info.ammo_type,
                                            inventory.ammo.size() - 1);
    return inventory.ammo[type] >= info.ammo_cost
        && inventory.available_weapons[beam];
}

bool PlayerAiData::check_charge(std::uint8_t beam) const noexcept {
    if (beam >= metadata::WeaponCount) {
        return false;
    }
    // The charge cost and the per-beam charge availability are carried by the
    // generated weapon record; the compact table only says whether the beam
    // can charge at all.
    return metadata::weapon_info(beam).can_charge;
}

bool PlayerAiData::is_player_visible(std::uint8_t viewer,
                                     std::uint8_t other) const noexcept {
    if (viewer >= visibility_.size() || other >= visibility_.size()) {
        return false;
    }
    return visibility_[other][viewer];
}



void PlayerAiData::update_target_item(const gameplay::Session& session,
                                      std::int32_t item_index) noexcept {
    const auto& items = session.items();
    const bool worth_it = item_index >= 0
        && static_cast<std::size_t>(item_index) < items.size()
        && items[static_cast<std::size_t>(item_index)].remaining_ticks != 0;
    if (worth_it) {
        flags2_ = flags2_ | AiFlags2::TargetItem;
    } else {
        flags2_ = static_cast<AiFlags2>(
            static_cast<std::uint32_t>(flags2_)
            & ~static_cast<std::uint32_t>(AiFlags2::TargetItem));
    }
    target_item_ = item_index;
}

void PlayerAiData::set_item_spawn(std::int32_t spawn_index) noexcept {
    // The managed helper keeps the spawn only when it has an item on it.
    item_spawn_ = spawn_index;
}

void PlayerAiData::find_entity_ref(AiEntRefType type) noexcept {
    // The seventy-eight-slot reference table is not populated by this head
    // yet; recording what was asked for keeps the behaviours honest about
    // what they wanted rather than pretending the lookup succeeded.
    last_entity_ref_ = type;
}

void PlayerAiData::execute_funcs1(const gameplay::Session& session,
                                  std::uint8_t bot_slot,
                                  int func_id) noexcept {
    const auto& table = ai_dispatch::funcs1_table;
    if (func_id < 0
        || static_cast<std::size_t>(func_id) >= std::size(table)) {
        return;
    }
    dispatch_funcs1(session, bot_slot,
                    table[static_cast<std::size_t>(func_id)]);
}

int PlayerAiData::evaluate_func3(const gameplay::Session& session,
                                  std::uint8_t bot_slot,
                                  const AiContext& context,
                                  int func_id,
                                  const ai::Parameters& parameters) noexcept {
    if (!session.has_player(bot_slot)) {
        return 0;
    }
    const auto& bot = session.player(bot_slot);
    const net::PlayerState* target =
        target_slot_ != 0xff && session.has_player(target_slot_)
        ? &session.player(target_slot_) : nullptr;
    const auto target_flag = has_flag(flags2_, AiFlags2::TargetPlayer);
    const auto parameter = static_cast<float>(parameters.param1) / 4096.0F;
    const auto target_distance = target == nullptr
        ? std::numeric_limits<float>::infinity()
        : distance_squared(bot.position, target->position);
    const auto inverse = [this](int value) { return value == 0 ? 1 : 0; };

    switch (func_id) {
    case 0:
        return 1;
    case 6:
    case 7:
    case 8:
        return inverse(target_flag ? 0 : 1);
    case 15:
        return target_flag ? 1 : 0;
    case 16:
        return inverse(target_flag ? 0 : 1);
    case 17:
        return target != nullptr && target_distance < parameter * parameter
            ? 1 : 0;
    case 18:
        return inverse(evaluate_func3(session, bot_slot, context, 17,
                                       parameters));
    case 35:
        return target != nullptr
            && (target->flags & net::PlayerState::FlagAltForm) != 0 ? 1 : 0;
    case 36:
        return inverse(evaluate_func3(session, bot_slot, context, 35,
                                       parameters));
    case 37:
        return target != nullptr
            && (target->flags & net::PlayerState::FlagFrozen) != 0 ? 1 : 0;
    case 38:
        return inverse(evaluate_func3(session, bot_slot, context, 37,
                                       parameters));
    case 39:
        return target_flag ? 1 : 0;
    case 40:
        return inverse(target_flag ? 0 : 1);
    case 45:
        return slot_hits_[bot_slot] != 0 ? 1 : 0;
    case 46:
        return target != nullptr
            && aggro_priority(4, 2, 1, target->slot_index, 0xff) != 0
            ? 1 : 0;
    case 47:
        return inverse(evaluate_func3(session, bot_slot, context, 46,
                                       parameters));
    case 49:
        return has_flag(flags2_, AiFlags2::Bit21) ? 1 : 0;
    case 50:
        return inverse(has_flag(flags2_, AiFlags2::Bit21) ? 1 : 0);
    case 53:
        return bot.health < session.inventory(bot_slot).health_max / 4
            ? 1 : 0;
    case 54:
        return inverse(evaluate_func3(session, bot_slot, context, 53,
                                      parameters));
    case 55:
        return bot.health == session.inventory(bot_slot).health_max ? 1 : 0;
    case 56:
        return inverse(evaluate_func3(session, bot_slot, context, 55,
                                      parameters));
    case 57:
        return bot.health < static_cast<std::uint16_t>(
            std::max(0.0F, parameter)) ? 1 : 0;
    case 58:
        return bot.health > static_cast<std::uint16_t>(
            std::max(0.0F, parameter)) ? 1 : 0;
    case 59:
        return bot.health;
    case 60:
        return std::max(0, static_cast<int>(
            session.inventory(bot_slot).health_max) - bot.health);
    case 68:
        return field118_ >= 302 ? 1 : 0;
    case 69:
        return field118_ > static_cast<std::uint32_t>(
            std::max(0.0F, parameter) * 2.0F) ? 1 : 0;
    case 70:
    case 71:
    case 72: {
        const auto next_depth = static_cast<std::size_t>(context.depth) + 1;
        if (next_depth >= execution_tree_.size()) {
            return 0;
        }
        return execution_tree_[next_depth].call_count
            > static_cast<std::uint32_t>(
                std::max(0.0F, parameter) * 2.0F) ? 1 : 0;
    }
    case 73:
        return slot_hits_[bot_slot];
    case 75:
        return slot_damage_[bot_slot];
    case 77:
        return damage_from_halfturret_;
    case 81:
        return player_aggro_count_ != 0 ? 1 : 0;
    case 82: {
        if (target == nullptr || target_distance >= 15.0F * 15.0F) {
            return 0;
        }
        const auto delta = subtract(target->position, bot.position);
        const float delta_length = std::sqrt(target_distance);
        const float facing_length = std::sqrt(length_squared(bot.facing));
        if (delta_length == 0.0F || facing_length == 0.0F) {
            return 1;
        }
        return dot(bot.facing, delta) / (facing_length * delta_length)
            > 0.5F ? 1 : 0;
    }
    case 83:
        return inverse(evaluate_func3(session, bot_slot, context, 82,
                                       parameters));
    case 84:
        return weapon1_ < session.inventory(bot_slot).available_weapons.size()
            && session.inventory(bot_slot).available_weapons[weapon1_]
            ? 1 : 0;
    case 85:
        return inverse(evaluate_func3(session, bot_slot, context, 84,
                                      parameters));
    case 92:
        return session.inventory(bot_slot).available_weapons[1] ? 1 : 0;
    case 93:
        return inverse(evaluate_func3(session, bot_slot, context, 92,
                                      parameters));
    case 98: {
        const auto weapon = metadata::weapon_info(weapon1_);
        const auto& inventory = session.inventory(bot_slot);
        return weapon.can_charge && inventory.available_weapons[weapon1_]
            && inventory.ammo[weapon.ammo_type] >= weapon.ammo_cost ? 1 : 0;
    }
    case 99:
        return inverse(evaluate_func3(session, bot_slot, context, 98,
                                      parameters));
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
    case 140:
    case 141:
    case 142:
    case 143:
    case 144:
    case 145:
    case 146:
    case 147:
    case 148:
    case 149:
    case 150:
    case 151: {
        const int weapon = (func_id - 134) / 2;
        const bool equal = weapon2_ == static_cast<std::uint8_t>(weapon);
        return (func_id & 1) == 0 ? (equal ? 1 : 0)
                                  : (equal ? 0 : 1);
    }
    case 176:
        return field118_ == static_cast<std::uint32_t>(
            std::max(0.0F, parameter)) ? 1 : 0;
    case 177:
        return inverse(evaluate_func3(session, bot_slot, context, 176,
                                      parameters));
    case 179:
        return bot.position.y < parameter ? 1 : 0;
    case 180:
        return bot.position.y >= parameter ? 1 : 0;
    case 181:
        return bot.position.x > parameter ? 1 : 0;
    case 182:
        return bot.position.x < parameter ? 1 : 0;
    case 183:
        return bot.position.z > parameter ? 1 : 0;
    case 184:
        return bot.position.z < parameter ? 1 : 0;
    case 185:
        return target != nullptr && target->position.y < parameter ? 1 : 0;
    case 186:
        return target != nullptr && target->position.y >= parameter ? 1 : 0;
    default:
        ++unknown_func3_calls_;
        return 0;
    }
}

void PlayerAiData::update_execution_path(const gameplay::Session* session,
                                          std::uint8_t bot_slot,
                                          const ai::Data1* node,
                                          std::size_t depth) noexcept {
    if (node == nullptr || depth >= execution_tree_.size()) {
        return;
    }
    auto& context = execution_tree_[depth];
    context.clear();
    context.data1 = node;
    context.depth = static_cast<std::uint8_t>(depth);
    // The node's Data3a behaviours run as the path is taken, before the
    // context's own identifier is read.
    // A null session means there is no bot to act on yet, which is only
    // the case before one has been created.
    if (session != nullptr) {
        for (const std::int32_t func_id : node->data3a) {
            execute_funcs1(*session, bot_slot, func_id);
        }
    }
    context.func24_id = node->func24_id;
    const auto count = std::min(node->data1.size(), context.weights.size() - 1);
    for (std::size_t i = 0; i < count; ++i) {
        context.weights[i] = 0;
    }
    if (!node->data1.empty()) {
        update_execution_path(session, bot_slot,
                              node->data1.front().get(), depth + 1);
    }
}

void PlayerAiData::execute(const gameplay::Session& session,
                           std::uint8_t bot_slot,
                           std::size_t depth) noexcept {
    if (depth >= execution_tree_.size()) {
        return;
    }
    auto& context = execution_tree_[depth];
    if (context.data1 == nullptr || !session.has_player(bot_slot)) {
        return;
    }
    // Execute runs the node's Data3b behaviours.
    for (const std::int32_t func_id : context.data1->data3b) {
        execute_funcs1(session, bot_slot, func_id);
    }
    context.field34 = session.player(bot_slot).position;
    if (context.call_count < std::numeric_limits<std::uint32_t>::max()) {
        ++context.call_count;
    }
    if (context.data1->data1.empty()
        || depth + 1 >= execution_tree_.size()) {
        return;
    }
    execute(session, bot_slot, depth + 1);
    const int child = update_path_weights(session, bot_slot, context);
    if (child >= 0
        && static_cast<std::size_t>(child) < context.data1->data1.size()) {
        update_execution_path(
            &session, bot_slot,
            context.data1->data1[static_cast<std::size_t>(child)].get(),
            depth + 1);
    }
}

int PlayerAiData::update_path_weights(const gameplay::Session& session,
                                      std::uint8_t bot_slot,
                                      AiContext& context) noexcept {
    const auto next_depth = static_cast<std::size_t>(context.depth) + 1;
    if (next_depth >= execution_tree_.size()) {
        return -1;
    }
    const ai::Data1* next = execution_tree_[next_depth].data1;
    if (next == nullptr || next->data2.empty()) {
        return -1;
    }
    int result = -1;
    for (const auto& data2 : next->data2) {
        bool no_update = false;
        for (const auto& data4 : data2.data4) {
            if (evaluate_func3(session, bot_slot, context,
                               data4.func3_id, data4.parameters) == 0) {
                no_update = true;
                break;
            }
        }
        if (no_update) {
            continue;
        }
        std::size_t weight_index = data2.data1_select_index < 20
            ? static_cast<std::size_t>(data2.data1_select_index)
            : context.data1->data1.size();
        if (weight_index >= context.weights.size()) {
            continue;
        }
        if (!((data2.weight >= 100000 && data2.func3_id != 210)
              || frame_count_ % 2 == 0)) {
            continue;
        }
        const int value = evaluate_func3(
            session, bot_slot, context, data2.func3_id, data2.parameters);
        const auto contribution = static_cast<std::int64_t>(value)
            * static_cast<std::int64_t>(data2.weight);
        const auto total = static_cast<std::int64_t>(context.weights[
            weight_index]) + contribution;
        context.weights[weight_index] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(
                total, std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::max()));
        if (context.weights[weight_index] >= 100000) {
            result = data2.data1_select_index;
            context.weights[weight_index] = 0;
            break;
        }
    }
    if (result >= 20) {
        const auto count = std::min(context.data1->data1.size(),
                                    context.weights.size());
        std::fill_n(context.weights.begin(), count, 0);
        return -1;
    }
    return result;
}

void PlayerAiData::process(const gameplay::Session& session,
                           std::uint8_t bot_slot) noexcept {
    if (!session.has_player(bot_slot)) {
        return;
    }
    update_visibility(session);
    update_aggro(session, bot_slot);
    flags2_ &= ~(AiFlags2::Bit18 | AiFlags2::Bit19
                 | AiFlags2::Bit20 | AiFlags2::AiStart
                 | AiFlags2::Bit16 | AiFlags2::Bit17
                 | AiFlags2::Bit21);
    flags4_ &= ~AiFlags4::Bit2;
    if (personality_ != nullptr && personality_->root != nullptr
        && execution_tree_[0].data1 == nullptr) {
        update_execution_path(nullptr, 0, personality_->root.get(),
                              0);
    }
    if (execution_tree_[0].data1 != nullptr) {
        execute(session, bot_slot, 0);
    }
    std::fill(slot_hits_.begin(), slot_hits_.end(), 0);
    std::fill(slot_damage_.begin(), slot_damage_.end(), 0);
    damage_from_halfturret_ = 0;
    flags2_ &= ~AiFlags2::AiStart;
    ++frame_count_;
}

BotDecision BotAi::decide(const gameplay::Session& session,
                          std::uint8_t bot_slot, int level) noexcept {
    BotDecision result;
    level = std::clamp(level, 0, 2);
    if (!session.has_player(bot_slot)) {
        return result;
    }

    const auto& bot = session.player(bot_slot);
    result.input.aim = bot.facing;
    if (!active_target(bot)) {
        return result;
    }

    const net::PlayerState* target = nullptr;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto& candidate : session.players()) {
        if (candidate.slot_index == bot_slot || !active_target(candidate)) {
            continue;
        }
        if (session.team_mode() && candidate.team == bot.team) {
            continue;
        }
        const float candidate_distance = distance_squared(
            bot.position, candidate.position);
        if (!std::isfinite(candidate_distance)
            || candidate_distance >= best_distance) {
            continue;
        }
        best_distance = candidate_distance;
        target = &candidate;
    }
    if (target == nullptr) {
        add_button(result.input, net::IntentButtons::MoveUp);
        const auto phase = (session.tick_count() / 45u + bot_slot) & 1u;
        add_button(result.input, phase == 0
            ? net::IntentButtons::MoveLeft : net::IntentButtons::MoveRight);
        return result;
    }

    result.target_slot = target->slot_index;
    result.target_distance = std::sqrt(std::max(0.0F, best_distance));
    result.input.aim = {
        target->position.x - bot.position.x,
        target->position.y - bot.position.y,
        target->position.z - bot.position.z
    };
    result.input.weapon_select = choose_weapon(
        session.inventory(bot_slot), result.target_distance, level);

    if (result.target_distance > 4.0F) {
        add_button(result.input, net::IntentButtons::MoveUp);
    } else if (result.target_distance < 2.0F) {
        add_button(result.input, net::IntentButtons::MoveDown);
    }
    const auto strafe_phase = (session.tick_count() / 30u + bot_slot) & 1u;
    add_button(result.input, strafe_phase == 0
        ? net::IntentButtons::MoveLeft : net::IntentButtons::MoveRight);

    if (level >= 2 && result.target_distance > 3.0F
        && (session.tick_count() + bot_slot * 13u) % 180u == 0u) {
        add_button(result.input, net::IntentButtons::Jump);
    }

    const std::uint32_t fire_period = level == 0 ? 30u
        : level == 1 ? 15u : 7u;
    if ((session.tick_count() + bot_slot * 7u) % fire_period == 0u) {
        add_button(result.input, net::IntentButtons::Shoot);
    }
    return result;
}

BotDecision BotAi::decide(const gameplay::Session& session,
                          std::uint8_t bot_slot,
                          PlayerAiData& state,
                          int level) noexcept {
    BotDecision result = decide(session, bot_slot, level);
    state.set_bot_level(level);
    state.process_input(result.input);
    state.process(session, bot_slot);
    result.target_slot = state.target_slot();
    result.aggro_score = state.aggro_score();
    result.personality_func24_id = state.personality_func24_id();
    result.ai_flags2 = static_cast<std::uint32_t>(state.flags2());
    result.ai_flags3 = static_cast<std::uint32_t>(state.flags3());
    result.ai_flags4 = static_cast<std::uint8_t>(state.flags4());
    if (result.target_slot != 0xff && session.has_player(result.target_slot)
        && session.has_player(bot_slot)) {
        const auto& bot = session.player(bot_slot);
        const auto& target = session.player(result.target_slot);
        result.target_distance = std::sqrt(std::max(
            0.0F, distance_squared(bot.position, target.position)));
    }
    return result;
}

namespace {

// PlayerAi._botLevelRandomValues1: per bot level, per hunter, how many frames
// a bot waits before it reacts.  Samus and Kanden are slower than the rest at
// every level.
constexpr std::array<std::array<std::uint32_t, 8>, 3> BotLevelAimDelays{{
    {{45, 45, 90, 45, 60, 45, 45, 45}},
    {{15, 15, 10, 10, 10, 10, 10, 10}},
    {{7, 7, 2, 2, 2, 2, 2, 2}}
}};

// PlayerAi._botLevelRandomValues2: how long a bot holds a decision before it
// reconsiders.
constexpr std::array<std::uint32_t, 3> BotLevelDecisionDelays{{150, 45, 10}};

} // namespace

void PlayerAiData::initialize_sub(std::uint8_t hunter, int bot_level,
                                  bool single_player,
                                  bool in_encounter) noexcept {
    if (single_player && in_encounter) {
        // A story bot that is already in a fight reacts with no delay at all.
        field102c_ = 0;
        field1030_ = 0;
        return;
    }
    // The cartridge clamps an out-of-range level to 1, not to the top level,
    // so an unknown level plays like a middling bot rather than the best one.
    const int index = bot_level < 0 || bot_level > 2 ? 1 : bot_level;
    const std::size_t hunter_index =
        hunter < BotLevelAimDelays[0].size() ? hunter : 0;
    // Doubled, because the native tick runs at twice the cartridge's rate.
    field102c_ =
        BotLevelAimDelays[static_cast<std::size_t>(index)][hunter_index] * 2;
    field1030_ = BotLevelDecisionDelays[static_cast<std::size_t>(index)] * 2;
}

void PlayerAiData::clear_input() noexcept {
    buttons_ = AiButtons{};
}

void PlayerAiData::initialize_at_load(const ai::Personality* personality,
                                      std::uint8_t hunter, int bot_level,
                                      bool single_player,
                                      bool in_encounter) noexcept {
    initialize(personality);
    clear_input();
    initialize_sub(hunter, bot_level, single_player, in_encounter);
    update_execution_path(
        nullptr, 0,
        personality != nullptr ? personality->root.get() : nullptr, 0);
}

void PlayerAiData::initialize_at_spawn(
    const ai::Personality* personality) noexcept {
    initialize(personality);
    clear_input();
    // Deliberately no initialize_sub: the delays are rolled once, when the bot
    // is created, so a bot does not get sharper or duller each time it dies.
    update_execution_path(
        nullptr, 0,
        personality != nullptr ? personality->root.get() : nullptr, 0);
}

std::string PlayerAiData::get_ouptut() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < execution_tree_.size(); ++i) {
        const AiContext& item = execution_tree_[i];
        if (item.data1 == nullptr) {
            break;
        }
        if (i == 0) {
            // The first line is the whole path, so a glance says where in the
            // tree the bot currently is.
            out << "\n";
            for (std::size_t j = 0; j < execution_tree_.size(); ++j) {
                const AiContext& node = execution_tree_[j];
                if (node.data1 == nullptr) {
                    break;
                }
                if (j != 0) {
                    out << " -> ";
                }
                out << node.data1->label;
                if (node.data1->data1.empty()) {
                    break;
                }
            }
            out << "\n";
            continue;
        }
        out << item.data1->label << "\n";
        const ai::Data1Ptr parent = item.data1->parent.lock();
        if (parent == nullptr || parent->data1.size() <= 1) {
            continue;
        }
        for (const ai::Data2& data2 : item.data1->data2) {
            int index = data2.data1_select_index;
            std::string target;
            if (index >= 20) {
                // Index 20 and up is the cartridge's "start over" branch
                // rather than a sibling.
                index = static_cast<int>(parent->data1.size());
                target = "reset";
            } else if (index >= 0
                       && static_cast<std::size_t>(index)
                              < parent->data1.size()) {
                target = parent->data1[static_cast<std::size_t>(index)]->label;
            } else {
                target = "?";
            }
            const std::int32_t weight =
                index >= 0
                        && static_cast<std::size_t>(index)
                               < execution_tree_[i - 1].weights.size()
                    ? execution_tree_[i - 1].weights[
                          static_cast<std::size_t>(index)]
                    : 0;
            const double percent =
                static_cast<double>(weight) / 100000.0 * 100.0;
            out << "w: " << std::setw(6) << weight << " / 100000 ("
                << std::fixed << std::setprecision(1) << std::setw(5)
                << percent << std::defaultfloat << "%) -> " << target << "\n";
        }
    }
    return out.str();
}

} // namespace fruityprime::players
