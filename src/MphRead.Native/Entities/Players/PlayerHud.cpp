// Native counterpart of src/MphRead/Entities/Players/PlayerHud.cs.
#include "PlayerHud.hpp"

#include "HUD/hud.hpp"
#include "Mods/Chat/ChatBox.hpp"
#include "Mods/Chat/PlayerEntityChatHud.hpp"
#include "Mods/Network/player_entity_net_hud.hpp"
#include "Strings.hpp"
#include "Metadata/metadata.hpp"
#include "Metadata/metadata_values.hpp"
#include "PlayerEntity.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fruityprime::players {
namespace {

// PlayerHud's own object layout for the hunter being drawn.
[[nodiscard]] const MphReadNative::Hud::ObjectPaths& objects_for(
    const HudContext& context) {
    return MphReadNative::Hud::elements().hunter_objects[
        std::min<std::size_t>(context.hunter, 7)];
}

// The managed HUD writes its colours as BGR555 literals.
[[nodiscard]] constexpr HudBackend::Color from_bgr555(
    std::uint16_t value) noexcept {
    return HudBackend::Color{
        static_cast<float>(value & 0x1F) / 31.0F,
        static_cast<float>((value >> 5) & 0x1F) / 31.0F,
        static_cast<float>((value >> 10) & 0x1F) / 31.0F};
}

// PlayerHud's scoreboard geometry.  The stock 28 fits the four players a DS
// match could hold; with more the list runs off both ends of a 192-unit
// screen, so it tightens to whatever fits and stops at the height of a
// hunter portrait.  The spacing constants are PlayerHud's own; the two
// methods that use them are below, beside the rest of the file's members.

[[nodiscard]] std::string call(
    const std::function<std::string(int)>& lookup, int id) {
    return lookup ? lookup(id) : std::string{};
}

} // namespace

void PlayerHud::DrawChat(
    PlayerEntity& player, const int viewport_width,
    const int viewport_height,
    const fruityprime::chat::player_entity_chat_hud::DrawText draw_text) {
    player.ModDrawChat(viewport_width, viewport_height, draw_text);
}

float PlayerHud::DrawText2D(const HudContext& context, float x, float y,
                            Align align, std::size_t palette,
                            std::string_view text,
                            const HudBackend::Color* color, float alpha,
                            float scale, int max_length) {
    if (context.backend == nullptr || text.empty()) {
        return x;
    }
    const auto& font = strings::Font::normal();
    const auto& widths = font.widths();
    const auto& offsets = font.offsets();
    if (widths.empty() || offsets.empty()) {
        return x;
    }
    if (max_length >= 0
        && static_cast<std::size_t>(max_length) < text.size()) {
        text = text.substr(0, static_cast<std::size_t>(max_length));
    }
    // A character the font does not carry is skipped rather than drawn as
    // whatever happens to sit at that index.
    const auto glyph = [&](char letter) -> int {
        const int index = static_cast<int>(
            static_cast<unsigned char>(letter)) - font.min_character();
        return index >= 0 && static_cast<std::size_t>(index) < widths.size()
            && static_cast<std::size_t>(index) < offsets.size()
            ? index : -1;
    };
    const auto run_width = [&]() {
        float total = 0.0F;
        for (const char letter : text) {
            const int index = glyph(letter);
            if (index >= 0) {
                total += static_cast<float>(
                    widths[static_cast<std::size_t>(index)]) * scale;
            }
        }
        return total;
    };
    if (align == Align::Right) {
        x -= run_width();
    } else if (align == Align::Center) {
        // Floored, the way the game's integer division does it: a character's
        // width includes its rightmost blank column, so halving the unfloored
        // total drifts the run left.
        x -= std::floor(run_width() * 0.5F);
    }
    // An explicit colour replaces every lit pixel, which is a different atlas
    // rather than a tint over the palette's green.
    const auto object = color != nullptr ? HudBackend::Object::FontMask
                                         : HudBackend::Object::Font;
    for (const char letter : text) {
        const int index = glyph(letter);
        if (index < 0) {
            continue;
        }
        const auto slot = static_cast<std::size_t>(index);
        if (letter != ' ') {
            const float top = y
                + static_cast<float>(offsets[slot]) * scale;
            static_cast<void>(context.backend->draw_hud_object(
                object, 0, slot, color != nullptr ? 0 : palette, x, top,
                scale, alpha, color));
        }
        x += static_cast<float>(widths[slot]) * scale;
    }
    return x;
}

std::string PlayerHud::FormatTime(float seconds) {
    const int rounded = std::max(0, static_cast<int>(seconds));
    const int minutes = rounded / 60;
    const int remaining = rounded % 60;
    return std::to_string(minutes) + ":"
        + std::string(1, static_cast<char>('0' + remaining / 10))
        + static_cast<char>('0' + remaining % 10);
}

std::string PlayerHud::FormatModeScore(const HudContext& context,
                                       std::uint8_t slot) {
    if (context.session == nullptr || !context.session->has_player(slot)) {
        return {};
    }
    const auto& player = context.session->player(slot);
    switch (context.mode) {
    case game::Mode::Battle:
    case game::Mode::BattleTeams:
    case game::Mode::Capture:
    case game::Mode::Nodes:
    case game::Mode::NodesTeams:
    case game::Mode::Bounty:
    case game::Mode::BountyTeams:
        return std::to_string(player.points) + " / "
            + std::to_string(context.state->point_goal);
    case game::Mode::Survival:
    case game::Mode::SurvivalTeams:
        return std::to_string(std::max(
            static_cast<int>(context.state->point_goal)
                - static_cast<int>(player.deaths), 0));
    default:
        return {};
    }
}

void PlayerHud::DrawMatchTime(const HudContext& context) {
    if (context.match_time_remaining < 0.0F) {
        return;
    }
    const std::size_t palette = context.match_time_remaining < 10.0F ? 2 : 0;
    const std::string title = call(context.hud_message, 5);
    if (!title.empty()) {
        static_cast<void>(DrawText2D(context, 128.0F, 10.0F, Align::Center,
                                     palette, title));
    }
    static_cast<void>(DrawText2D(
        context, 128.0F, 20.0F, Align::Center, palette,
        FormatTime(context.match_time_remaining)));
}

void PlayerHud::DrawScoreboardPlayer(const HudContext& context, float x,
                                     float y,
                                     const HudBackend::Color& color,
                                     std::uint8_t slot, std::size_t hunter) {
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::HunterPortrait, hunter, 0, 0, x - 40.0F,
        y - 13.0F, 1.0F, 1.0F, nullptr));
    const std::size_t stars = std::min<std::size_t>(
        static_cast<std::size_t>(context.state->stars[slot]), 7);
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::Stars, 0, stars * 2, 0, x, y, 1.0F, 1.0F,
        nullptr));
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::Stars, 0, stars * 2 + 1, 0, x + 32.0F, y, 1.0F,
        1.0F, nullptr));
    const std::string nickname =
        context.nickname ? context.nickname(slot) : std::string{};
    if (!nickname.empty()) {
        static_cast<void>(DrawText2D(context, x + 32.0F, y - 9.0F,
                                     Align::Center, 0, nickname, &color));
    }
}

void PlayerHud::DrawScoreboard(const HudContext& context) {
    if (context.session == nullptr || context.state == nullptr) {
        return;
    }
    const auto columns = net::hud::score_columns(context.networked);
    const float row_space = GetScoreboardRowSpace(context);
    float y = 104.0F - GetScoreboardHeight(context) / 2.0F;
    const HudBackend::Color header_color = from_bgr555(0x3FEF);
    if (context.match_over) {
        const std::string over = call(context.hud_message, GameOverMessageId);
        if (!over.empty()) {
            const HudBackend::Color over_color = from_bgr555(0x53F4);
            static_cast<void>(DrawText2D(context, 128.0F, y, Align::Center, 0,
                                         over, &over_color));
        }
        y += ScoreStartSpace;
    }
    static_cast<void>(DrawText2D(
        context, columns.first, y, Align::Center, 0,
        net::hud::score_header_one(context.mode), &header_color));
    static_cast<void>(DrawText2D(
        context, columns.second, y, Align::Center, 0,
        net::hud::score_header_two(context.mode), &header_color));
    if (context.networked) {
        static_cast<void>(DrawText2D(context, columns.ping, y, Align::Center,
                                     0, "ping", &header_color));
    }
    y += ScoreStartSpace;

    for (std::size_t index = 0; index < context.state->active_players;
         ++index) {
        const auto slot = static_cast<std::uint8_t>(
            context.state->result_slots[index]);
        if (!context.session->has_player(slot)) {
            continue;
        }
        // The local player's row pulses white to blue over about a second,
        // which is how you find yourself in a list of eight.
        HudBackend::Color color = from_bgr555(0x7DEF);
        if (slot == context.local_slot) {
            const float pct = std::fmod(context.elapsed_seconds
                                        / (32.0F / 30.0F), 1.0F);
            const float channel = pct <= 0.5F ? pct * 2.0F
                                              : 1.0F - (pct - 0.5F) * 2.0F;
            color = HudBackend::Color{channel, channel, 1.0F};
        }
        auto hunter = static_cast<std::size_t>(
            context.session->player_profile(slot).hunter);
        if (hunter >= metadata::HunterCount) {
            hunter = 0;
        }
        DrawScoreboardPlayer(context, 60.0F, y, color, slot, hunter);
        static_cast<void>(DrawText2D(
            context, columns.first, y, Align::Center, 0,
            net::hud::score_value_one(*context.state, context.mode, slot),
            &color));
        static_cast<void>(DrawText2D(
            context, columns.second, y, Align::Center, 0,
            net::hud::score_value_two(*context.state, context.mode, slot),
            &color));
        if (context.networked && context.ping) {
            const auto label = net::hud::ping_label(context.ping(slot));
            const auto rgb = net::hud::ping_rgb(label.tone);
            const HudBackend::Color ping_color{rgb[0], rgb[1], rgb[2]};
            static_cast<void>(DrawText2D(context, columns.ping, y,
                                         Align::Center, 0, label.text,
                                         &ping_color));
        }
        y += row_space;
    }
}

void PlayerHud::DrawFps(const HudContext& context) {
    // The number sits in the right-hand corner with a smaller "fps" hung off
    // its right edge, so a run that gains a digit at a hundred grows leftwards
    // instead of off the screen.  A glyph is placed from its top, so the
    // smaller run comes down by the difference to share a baseline.
    constexpr float Margin = 3.0F;
    constexpr float Top = 3.0F;
    constexpr float NumberScale = 0.5F;
    constexpr float UnitScale = 0.34F;
    const HudBackend::Color color = from_bgr555(0x3FEF);
    const float unit_x = DrawText2D(
        context, 256.0F - Margin, Top + (NumberScale - UnitScale) * 8.0F,
        Align::Right, 0, "fps", &color, 1.0F, UnitScale);
    static_cast<void>(DrawText2D(
        context, unit_x - 1.0F, Top, Align::Right, 0,
        std::to_string(static_cast<int>(context.frames_per_second + 0.5F)),
        &color, 1.0F, NumberScale));
}

void PlayerHud::DrawModeRules(const HudContext& context) {
    const auto& table = MphReadNative::Hud::elements().rules;
    std::size_t index = 0;
    switch (context.mode) {
    case game::Mode::Survival:
    case game::Mode::SurvivalTeams: index = 1; break;
    case game::Mode::PrimeHunter: index = 2; break;
    case game::Mode::Bounty:
    case game::Mode::BountyTeams: index = 3; break;
    case game::Mode::Capture: index = 4; break;
    case game::Mode::Defender:
    case game::Mode::DefenderTeams: index = 5; break;
    case game::Mode::Nodes:
    case game::Mode::NodesTeams: index = 6; break;
    default: index = 0; break;
    }
    const auto& rules = table[index];
    const std::string header = call(context.rules_message,
                                    rules.message_ids[0]);
    if (header.empty()) {
        return;
    }
    const HudBackend::Color header_color = from_bgr555(0x7FDE);
    const HudBackend::Color body_color = from_bgr555(0x7F5A);
    static_cast<void>(DrawText2D(context, 128.0F, 10.0F, Align::Center, 0,
                                 header, &header_color));
    // ElapsedTime / (1/30): the body appears at the rate the cartridge types
    // it, counted from the start of the sequence rather than from the frame.
    const int typed = static_cast<int>(context.intro_seconds * 30.0F);
    int consumed = 0;
    float y = 28.0F;
    for (int line_index = 1; line_index < rules.count; ++line_index) {
        const std::string line = call(
            context.rules_message,
            rules.message_ids[static_cast<std::size_t>(line_index)]);
        const int remaining = typed - consumed;
        if (remaining <= 0) {
            break;
        }
        static_cast<void>(DrawText2D(
            context,
            static_cast<float>(
                rules.offsets[static_cast<std::size_t>(line_index)]) + 12.0F,
            y, Align::Left, 0, line, &body_color, 1.0F, 1.0F, remaining));
        consumed += static_cast<int>(line.size());
        y += 13.0F;
    }
    // PlayerProcess draws 245 rather than 244 while the intro is running.
    const std::string begin = call(context.hud_message, 245);
    if (!begin.empty()) {
        const HudBackend::Color message_color = from_bgr555(0x3FEF);
        static_cast<void>(DrawText2D(context, 128.0F, 162.0F, Align::Center,
                                     0, begin, &message_color));
    }
}

void PlayerHud::DrawRespawnPrompt(const HudContext& context) {
    if (context.session == nullptr || context.state == nullptr
        || context.state->single_player) {
        return;
    }
    const HudBackend::Color color = from_bgr555(0x3FEF);
    const std::string prompt = call(context.hud_message, 244);
    if (!prompt.empty()) {
        static_cast<void>(DrawText2D(context, 128.0F, 162.0F, Align::Center,
                                     0, prompt, &color));
    }
    // The countdown only appears in the last five seconds; before that the
    // number would be a distraction from a fight still going on.
    const auto remaining = context.session->respawn_ticks(context.local_slot);
    if (remaining == 0 || remaining >= 300) {
        return;
    }
    std::string message = call(context.hud_message, 246);
    if (message.empty()) {
        return;
    }
    const auto seconds = std::to_string((remaining + 60) / 60);
    if (const auto at = message.find("%d"); at != std::string::npos) {
        message.replace(at, 2, seconds);
    }
    static_cast<void>(DrawText2D(context, 128.0F, 152.0F, Align::Center, 0,
                                 message, &color));
}


bool PlayerHud::DrawMeter(const HudContext& context,
                          HudBackend::Object object, float x, float y,
                          int base_amount, int current_amount,
                          const MphReadNative::Hud::Meter& meter,
                          std::size_t palette, bool draw_text, float alpha) {
    if (meter.length <= 0 || context.backend == nullptr) {
        return false;
    }
    const int tank_amount = std::max(meter.tank_amount, 1);
    // Multiplayer takes the smaller of the two; the story subtracts whole
    // tanks first, which this head has no tanks to subtract.
    const int bar_amount = std::min(base_amount, current_amount);
    const int tiles = (meter.length + 7) / 8;
    const int denominator = static_cast<int>(
        (99000LL * tank_amount) / meter.length);
    int filled = denominator > 0
        ? static_cast<int>((100000LL * std::max(bar_amount, 0)) / denominator)
        : 0;
    if (filled == 0 && bar_amount > 0) {
        filled = 1;
    }
    filled = std::clamp(filled, 0, meter.length);
    float tile_x = x;
    float tile_y = y;
    bool drawn = false;
    for (int tile = 0; tile < tiles; ++tile) {
        std::size_t frame = 8;
        if (tile < filled / 8) {
            frame = 0;
        } else if (tile == filled / 8 && (filled & 7) != 0) {
            frame = static_cast<std::size_t>(8 - (filled & 7));
        }
        drawn = context.backend->draw_hud_object(
            object, 0, frame, palette, tile_x, tile_y, 1.0F, alpha, nullptr)
            || drawn;
        if (meter.horizontal) {
            tile_x += 8.0F;
        } else {
            tile_y -= 8.0F;
        }
    }
    if (draw_text) {
        // Two digits wide, so a single-digit amount keeps its column instead
        // of sliding about.
        const int clamped = std::clamp(current_amount, 0, 99);
        std::string amount;
        amount += static_cast<char>('0' + clamped / 10);
        amount += static_cast<char>('0' + clamped % 10);
        static_cast<void>(DrawText2D(
            context, x + static_cast<float>(meter.bar_offset_x),
            y + static_cast<float>(meter.bar_offset_y), meter.align, palette,
            amount, nullptr, alpha));
        if (meter.message_id > 0) {
            const std::string message = call(context.hud_message,
                                             meter.message_id);
            if (!message.empty()) {
                static_cast<void>(DrawText2D(
                    context, x + static_cast<float>(meter.text_offset_x),
                    y + static_cast<float>(meter.text_offset_y), Align::Left,
                    palette, message, nullptr, alpha));
            }
        }
    }
    return drawn;
}

float PlayerHud::UpdateHealthbarOffset(const HudContext& context,
                                       float current,
                                       bool alt_form) noexcept {
    const auto& objects =
        MphReadNative::Hud::elements().hunter_objects[
            std::min<std::size_t>(context.hunter, 7)];
    const float target = static_cast<float>(objects.health_offset_y)
        + (alt_form ? static_cast<float>(objects.health_offset_y_alt) : 0.0F);
    if (current > target) {
        return std::max(target, current - 0.5F);
    }
    if (current < target) {
        return std::min(target, current + 0.5F);
    }
    return current;
}

bool PlayerHud::DrawHealthbars(const HudContext& context,
                               const MphReadNative::Hud::PlayerState& player,
                               float healthbar_y_offset) {
    const auto hunter = std::min<std::size_t>(context.hunter, 7);
    const auto& elements = MphReadNative::Hud::elements();
    const auto& objects = elements.hunter_objects[hunter];
    const std::size_t palette = player.health < 25 ? 2 : 0;
    const auto& main = elements.main_healthbars[hunter];
    bool drawn = false;
    if (player.health > 0) {
        drawn = DrawMeter(
            context, HudBackend::Object::HealthBar,
            static_cast<float>(objects.health_main_pos_x),
            static_cast<float>(objects.health_main_pos_y)
                + healthbar_y_offset,
            main.tank_amount - 1, player.health, main, palette,
            /*draw_text=*/true);
    }
    // Multiplayer's second bar carries whatever is above one tank.
    if (static_cast<int>(player.health) > main.tank_amount) {
        const auto& sub = elements.sub_healthbars[hunter];
        static_cast<void>(DrawMeter(
            context, HudBackend::Object::HealthBarSub,
            static_cast<float>(objects.health_sub_pos_x),
            static_cast<float>(objects.health_sub_pos_y)
                + healthbar_y_offset,
            main.tank_amount - 1,
            static_cast<int>(player.health) - main.tank_amount, sub, palette,
            /*draw_text=*/false));
    }
    return drawn;
}

void PlayerHud::DrawAmmoBar(const HudContext& context,
                            const MphReadNative::Hud::PlayerState& player,
                            const gameplay::InventoryState& inventory) {
    const auto& weapon = metadata::weapon_info(player.current_weapon);
    const auto hunter = std::min<std::size_t>(context.hunter, 7);
    const auto& elements = MphReadNative::Hud::elements();
    const auto& objects = elements.hunter_objects[hunter];
    if (weapon.ammo_cost > 0) {
        auto meter = elements.ammo_bars[hunter];
        const auto type = std::min<std::size_t>(weapon.ammo_type,
                                                inventory.ammo.size() - 1);
        const int amount = static_cast<int>(inventory.ammo[type]);
        meter.tank_amount = static_cast<int>(inventory.ammo_max[type]) + 1;
        meter.tank_count = 0;
        static_cast<void>(DrawMeter(
            context, HudBackend::Object::AmmoBar,
            static_cast<float>(objects.ammo_bar_pos_x),
            static_cast<float>(objects.ammo_bar_pos_y), amount, amount, meter,
            0, /*draw_text=*/false));
        // The bar is the raw amount; the number beside it is how many shots
        // that is, which is not the same for a weapon costing more than one.
        const int shots = amount / std::max<int>(weapon.ammo_cost, 1);
        const int clamped = std::clamp(shots, 0, 99);
        std::string text;
        text += static_cast<char>('0' + clamped / 10);
        text += static_cast<char>('0' + clamped % 10);
        static_cast<void>(DrawText2D(
            context,
            static_cast<float>(objects.ammo_bar_pos_x + meter.bar_offset_x),
            static_cast<float>(objects.ammo_bar_pos_y + meter.bar_offset_y),
            meter.align, 0, text));
    }
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::WeaponIcon, 0, player.current_weapon, 0,
        static_cast<float>(objects.weapon_icon_pos_x),
        static_cast<float>(objects.weapon_icon_pos_y), 1.0F, 1.0F, nullptr));
}

void PlayerHud::DrawModeScore(const HudContext& context, int message_id,
                              const std::string& text) {
    if (text.empty()) {
        return;
    }
    const auto& objects = MphReadNative::Hud::elements().hunter_objects[
        std::min<std::size_t>(context.hunter, 7)];
    const float x = static_cast<float>(objects.score_pos_x);
    float y = fruityprime::chat::player_entity_chat_hud::clearance(
        fruityprime::chat::ChatBox::Available(),
        static_cast<float>(objects.score_pos_y));
    const std::string message = call(context.hud_message, message_id);
    // The game wraps here, but the text used never wraps.
    static_cast<void>(DrawText2D(context, x, y, objects.score_align, 0,
                                 message));
    y += 9.0F;
    static_cast<void>(DrawText2D(context, x, y, objects.score_align, 0,
                                 text));
}

MphReadNative::Hud::PlayerState PlayerHud::build(
    const gameplay::Session& session, std::uint8_t slot) {
    return MphReadNative::Hud::player_state(session, slot);
}

PlayerHud::HudUpdate PlayerHud::update_hud(const HudFrame& frame) noexcept {
    HudUpdate update;
    if (frame.menu_pause) {
        // The pause menu takes the visor layer for its background and leaves
        // the helmet front drawing over it; nothing else updates, so the
        // readouts freeze rather than animating behind the menu.
        update.draw_pause_background = true;
        return update;
    }
    if (frame.dialog_pause) {
        // A dialog stops time without drawing anything of its own here.
        return update;
    }
    update.update_readouts = true;
    update.update_dialogs = frame.single_player;
    update.update_weapon_menu = frame.weapon_menu_open;
    return update;
}

PlayerHud::ModeHud PlayerHud::process_mode_hud(game::Mode mode) noexcept {
    switch (mode) {
    case game::Mode::Survival:
    case game::Mode::SurvivalTeams:
        return ModeHud::Survival;
    case game::Mode::Bounty:
    case game::Mode::BountyTeams:
        return ModeHud::Bounty;
    case game::Mode::Capture:
        return ModeHud::Capture;
    case game::Mode::Defender:
    case game::Mode::DefenderTeams:
        return ModeHud::Defender;
    case game::Mode::Nodes:
    case game::Mode::NodesTeams:
        return ModeHud::Nodes;
    case game::Mode::PrimeHunter:
        return ModeHud::PrimeHunter;
    default:
        // Battle and the story have no mode HUD of their own; the opponent
        // marker every mode draws is not part of this dispatch.
        return ModeHud::None;
    }
}

PlayerHud::HudDraw PlayerHud::draw_hud_objects(
    const DrawFrame& frame) noexcept {
    HudDraw draw;
    if (frame.thumbnail_mode) {
        // A thumbnail is a picture of the world, not of somebody playing.
        return draw;
    }
    // Before every gate below: the switch for the counter lives in the
    // settings window, so a counter that vanished while it was open would be
    // a switch with no feedback.
    draw.fps = frame.show_fps;
    // Likewise: somebody watching a match is in the one position where reading
    // what the players are saying is most of the point.
    draw.chat = true;
    if (frame.free_camera) {
        // Looking at the map, not out of anybody's eyes -- there is no player
        // whose readouts these would be.  The scoreboard is the exception,
        // because it belongs to the match rather than to a player.
        if (frame.show_scoreboard && !frame.menu_pause) {
            draw.match_time = true;
            draw.scoreboard = true;
        }
        return draw;
    }
    if (frame.menu_pause) {
        return draw;
    }
    if (frame.match_state == game::MatchState::GameOver) {
        draw.game_over_text = true;
    }
    draw.player_hud = true;
    if (frame.show_scoreboard) {
        draw.match_time = true;
        draw.scoreboard = true;
    }
    return draw;
}

namespace {

// PlayerHud.SetUpFont, minus the half that swaps the drawing font over:
// wrapping only needs to know which set of widths to measure with.
[[nodiscard]] const strings::Font& font_for(char first,
                                            bool japanese) noexcept {
    const auto lead = static_cast<unsigned char>(first);
    if (japanese && (lead & 0xA0u) == 0xA0u) {
        return strings::Font::kanji();
    }
    return strings::Font::normal();
}

} // namespace

int HudMessageQueue::WrapText(std::string_view text, int max_width,
                              std::string& destination, bool japanese) {
    destination.clear();
    int lines = 1;
    if (max_width <= 0) {
        return lines;
    }
    if (text.empty()) {
        return 1;
    }
    // The managed method writes into a caller's buffer twice the length of
    // the text, because a break can be inserted rather than replacing a
    // space.  Reserving that much up front keeps the indices below meaning
    // the same thing they do there.
    destination.assign(text.size() * 2 + 1, '\0');

    int line_width = 0;
    // How much width the line after a break already holds.  Zeroed when we
    // break without a space to break at, since the new line then starts
    // empty.
    int width_after_break = 0;
    std::size_t break_position = 0;
    std::size_t c = 0;
    const auto& font = font_for(text[0], japanese);
    const auto& widths = font.widths();

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char letter = text[i];
        const auto raw = static_cast<unsigned char>(letter);
        destination[c] = letter;
        if (letter == '\n') {
            line_width = 0;
            break_position = 0;
            width_after_break = 0;
            ++lines;
        } else {
            if (letter == ' ') {
                break_position = c;
                width_after_break = 0;
            }
            if (raw >= static_cast<unsigned char>(' ')) {
                int index = static_cast<int>(raw);
                if ((raw & 0x80u) != 0 && i + 1 < text.size()) {
                    // A two-byte character: the pair is copied through and
                    // measured as the one glyph it is.
                    const char next = text[++i];
                    destination[++c] = next;
                    index = (static_cast<unsigned char>(next) & 0x3F)
                        | ((raw & 0x1F) << 6);
                }
                index -= font.min_character();
                if (index >= 0
                    && static_cast<std::size_t>(index) < widths.size()) {
                    const int width = widths[static_cast<std::size_t>(index)];
                    line_width += width;
                    if (letter != ' ') {
                        width_after_break += width;
                    }
                }
            }
            if (i + 1 < text.size() && line_width > max_width) {
                if (break_position == 0 && max_width >= 8) {
                    // No space to break at, so the break goes after the
                    // character just written and the new line starts empty.
                    break_position = c + 1;
                    ++c;
                    width_after_break = 0;
                }
                if (break_position > 0) {
                    destination[break_position] = '\n';
                    line_width = width_after_break;
                    width_after_break = 0;
                    break_position = 0;
                    ++lines;
                }
            }
        }
        ++c;
    }
    destination.resize(c);
    return lines;
}

void HudMessageQueue::QueueHudMessage(float x, float y, float duration,
                                      std::uint8_t category,
                                      std::string_view text,
                                      bool dialog_hide) {
    QueueHudMessage(x, y, Align::Center, 256, 8.0F, DefaultColor, 1.0F,
                    duration, category, text, dialog_hide);
}

void HudMessageQueue::QueueHudMessage(float x, float y, int max_width,
                                      float duration, std::uint8_t category,
                                      std::string_view text,
                                      bool dialog_hide) {
    QueueHudMessage(x, y, Align::Center, max_width, 8.0F, DefaultColor, 1.0F,
                    duration, category, text, dialog_hide);
}

void HudMessageQueue::QueueHudMessage(float x, float y, Align align,
                                      int max_width, float font_size,
                                      const HudBackend::Color& color,
                                      float alpha, float duration,
                                      std::uint8_t category,
                                      std::string_view text,
                                      bool dialog_hide) {
    std::string wrapped;
    const int line_count = WrapText(text, max_width, wrapped, japanese_);
    // The slot with the least life left is the one reused, which is how the
    // cartridge keeps a fixed twenty without ever refusing a message.
    float least = std::numeric_limits<float>::max();
    HudMessage* target = nullptr;
    for (auto& existing : messages_) {
        if (existing.lifetime > 0.0F) {
            if ((category & existing.category & 14) != 0) {
                // Messages in the same stack push each other up rather than
                // overlapping.
                existing.y -= static_cast<float>(line_count)
                    * existing.font_size;
            } else if (existing.y == y) {
                existing.lifetime = 0.0F;
            }
        }
        if (existing.lifetime < least) {
            least = existing.lifetime;
            target = &existing;
        }
    }
    if (target == nullptr) {
        return;
    }
    if ((category & 14) != 0) {
        y -= static_cast<float>(line_count - 1) * font_size;
    }
    target->text = std::move(wrapped);
    target->x = x;
    target->y = y;
    target->max_width = max_width;
    target->font_size = font_size;
    target->color = color;
    target->alpha = alpha;
    target->align = align;
    target->category = category;
    target->lifetime = duration;
    target->dialog_hide = dialog_hide;
}

void HudMessageQueue::ClearHudMessage(int mask) noexcept {
    for (auto& message : messages_) {
        if ((mask & message.category) != 0) {
            message.lifetime = 0.0F;
        }
    }
}

bool HudMessageQueue::IsHudMessageQueued(int mask) const noexcept {
    for (const auto& message : messages_) {
        if ((mask & message.category) != 0 && message.lifetime > 0.0F) {
            return true;
        }
    }
    return false;
}

void HudMessageQueue::ProcessHudMessageQueue(float frame_time) noexcept {
    for (auto& message : messages_) {
        if (message.lifetime > 0.0F) {
            message.lifetime -= frame_time;
            if (message.lifetime < 0.0F) {
                message.lifetime = 0.0F;
            }
        }
    }
}

void HudMessageQueue::DrawQueuedHudMessages(const HudContext& context,
                                            const DrawFrame& frame) const {
    if (frame.menu_pause) {
        return;
    }
    for (const auto& message : messages_) {
        if (message.lifetime <= 0.0F) {
            continue;
        }
        // Category bit 0 blinks: shown for four frames out of every
        // fourteen.  The cartridge counts at half this rate.
        if ((message.category & 1) != 0
            && (frame.frame_count & (7u * 2u)) > 3u * 2u) {
            continue;
        }
        if (frame.dialog_pause && message.dialog_hide) {
            continue;
        }
        // The font size is the scale the run is drawn at; eight is one to
        // one, which is what every caller but the mode announcements uses.
        static_cast<void>(PlayerHud::DrawText2D(
            context, message.x, message.y, message.align, 0, message.text,
            &message.color, message.alpha, message.font_size / 8.0F));
    }
}

float PlayerHud::GetScoreboardRowSpace(const HudContext& context) {
    if (context.state == nullptr) {
        return ScorePlayerSpace;
    }
    const int rows = static_cast<int>(context.state->active_players);
    if (rows <= 4) {
        return ScorePlayerSpace;
    }
    float available = 168.0F - ScoreStartSpace;
    if (context.match_over) {
        // GAME OVER takes a row of its own above the list.
        available -= ScoreStartSpace;
    }
    if (context.state->teams) {
        available -= 2.0F * ScoreTeamLineSpace;
    }
    return std::clamp(available / static_cast<float>(rows),
                      ScoreMinPlayerSpace, ScorePlayerSpace);
}

float PlayerHud::GetScoreboardHeight(const HudContext& context) {
    if (context.state == nullptr) {
        return ScoreStartSpace;
    }
    const float row_space = GetScoreboardRowSpace(context);
    float height = ScoreStartSpace;
    if (context.match_over) {
        height *= 2.0F;
    }
    // Four is "no team yet": the first player always opens one.
    int current_team = 4;
    const std::size_t count = std::min<std::size_t>(
        context.state->active_players, context.state->result_slots.size());
    for (std::size_t index = 0; index < count; ++index) {
        const std::int32_t slot = context.state->result_slots[index];
        if (slot < 0 || context.session == nullptr
            || !context.session->has_player(
                static_cast<std::uint8_t>(slot))) {
            continue;
        }
        if (context.state->teams) {
            const int team = static_cast<int>(
                context.state->player_teams[static_cast<std::size_t>(slot)]);
            if (team != current_team) {
                if (current_team != 4) {
                    height -= ScoreTeamHeaderSpace;
                }
                height += ScoreTeamLineSpace;
                current_team = team;
            }
        }
        height += row_space;
    }
    return height;
}

void PlayerHud::DrawEscapeTime(const HudContext& context,
                               const float seconds, const float shift_x,
                               const float shift_y) {
    if (seconds < 0.0F) {
        return;
    }
    const int total = static_cast<int>(seconds);
    const int minutes = total / 60;
    const int remainder = total % 60;
    // Hundredths, truncated the way the managed division is.
    const int hundredths = static_cast<int>(
        (seconds - static_cast<float>(total)) * 100.0F);
    char text[32];
    std::snprintf(text, sizeof(text), "%d:%02d:%02d", minutes, remainder,
                  hundredths);
    // The last ten seconds turn red.
    const std::size_t palette = seconds < 10.0F ? 2u : 0u;
    static_cast<void>(DrawText2D(context, 128.0F + shift_x,
                                 180.0F + shift_y, Align::Center, palette,
                                 text));
}

PlayerHud::LocatorPlacement PlayerHud::PlaceLocatorIcon(
    const net::Vec3& view, const float projected_x, const float projected_y,
    const float width, const float height) noexcept {
    // The HUD is laid out in the DS's 256x192 space and stretched to the
    // window, so every constant below is converted through these two.
    const auto w = [width](float value) {
        return value / 256.0F * width;
    };
    const auto h = [height](float value) {
        return value / 192.0F * height;
    };
    LocatorPlacement placement;
    float x = 0.0F;
    float y = 0.0F;
    bool behind = false;
    float px = 0.0F;
    float py = 0.0F;
    if (view.z < -1.0F) {
        px = projected_x * width;
        py = projected_y * height;
        x = px - w(128.0F);
        y = py - h(106.0F);
    } else {
        // Behind the camera, where a projection means nothing: the view
        // space offsets stand in for it and the marker is always pinned.
        x = w(view.x);
        y = -h(view.y);
        behind = true;
    }
    const float abs_x = std::fabs(x);
    const float abs_y = std::fabs(y);
    if (!behind && abs_x <= w(100.0F) && abs_y <= h(60.0F)) {
        placement.x = px / width;
        placement.y = py / height;
        return placement;
    }
    if (abs_y >= 1.0F / 4096.0F) {
        // Where the line out to the marker crosses the box: the top or
        // bottom edge first, and the left or right one if it leaves through
        // there instead.
        const float crossing_x =
            abs_x + std::trunc((h(60.0F) - abs_y) * abs_x / abs_y);
        if (crossing_x > w(100.0F)) {
            const float crossing_y =
                abs_y + std::trunc((w(100.0F) - abs_x) * abs_y / abs_x);
            px = x <= 0.0F ? w(28.0F) : w(228.0F);
            py = y <= 0.0F ? h(106.0F) - crossing_y
                           : crossing_y + h(106.0F);
        } else {
            px = x <= 0.0F ? w(128.0F) - crossing_x
                           : crossing_x + w(128.0F);
            py = y <= 0.0F ? h(46.0F) : h(166.0F);
        }
    } else {
        // Level with the middle of the box, so only the side matters and
        // the height is left wherever the projection put it -- which is
        // zero when the marker is behind the camera.
        px = x <= 0.0F ? w(28.0F) : w(228.0F);
    }
    placement.x = px / width;
    placement.y = py / height;
    placement.angle = std::atan2(-y, x) * 180.0F
        / 3.14159265358979323846F;
    placement.arrow = true;
    return placement;
}

namespace {

// The three colours the mode HUDs mark things with, in the cartridge's own
// BGR555.  "Good" is the blue-white a friendly marker gets; a hostile one
// is plain red.
constexpr HudBackend::Color LocatorNeutral{1.0F, 1.0F, 1.0F};
constexpr HudBackend::Color LocatorGood{
    15.0F / 31.0F, 15.0F / 31.0F, 1.0F};
constexpr HudBackend::Color LocatorHostile{1.0F, 0.0F, 0.0F};

// Metadata.TeamColors, converted the same way.
[[nodiscard]] HudBackend::Color team_color(int team) noexcept {
    const auto index = static_cast<std::size_t>(team);
    if (index >= metadata::TeamColors.size()) {
        return LocatorNeutral;
    }
    const auto& color = metadata::TeamColors[index];
    return {static_cast<float>(color.red) / 31.0F,
            static_cast<float>(color.green) / 31.0F,
            static_cast<float>(color.blue) / 31.0F};
}

// The team a slot is on, or 4 for "none".
[[nodiscard]] int team_of(const HudContext& context,
                          std::uint8_t slot) noexcept {
    if (context.state == nullptr
        || slot >= context.state->player_teams.size()) {
        return 4;
    }
    return static_cast<int>(context.state->player_teams[slot]);
}

// Whichever of a flag's two positions is the live one.
[[nodiscard]] net::Vec3 point_of(
    const scene::VolumePoint& point) noexcept {
    return {point.x, point.y, point.z};
}

[[nodiscard]] float lerp(float first, float second, float by) noexcept {
    return first * (1.0F - by) + second * by;
}

} // namespace

void PlayerHud::AddLocatorInfo(ModeHudState& state,
                               const net::Vec3& position,
                               const LocatorIcon icon,
                               const HudBackend::Color& color,
                               const float alpha) {
    state.locators.push_back({position, icon, color, alpha});
}

void PlayerHud::ProcessHudSurvival(const HudContext& context,
                                   const ModeHudFrame& frame,
                                   ModeHudState& state, int& reveal) {
    if (context.session == nullptr) {
        return;
    }
    const int own_team = team_of(context, context.local_slot);
    for (const auto& player : context.session->players()) {
        if (player.health == 0
            || team_of(context, player.slot_index) == own_team) {
            continue;
        }
        float alpha = 1.0F;
        const std::size_t slot = player.slot_index;
        if (frame.radar_players) {
            // Every opponent is on the radar, pulsing in and out over
            // thirty-two of the cartridge's frames.
            if (slot < frame.reveal_elapsed.size()) {
                const float past = frame.reveal_elapsed[slot];
                const float period = 32.0F / 30.0F;
                float fraction = std::fmod(past / period, 1.0F);
                if (fraction < 0.0F) {
                    fraction += 1.0F;
                }
                alpha = fraction <= 0.5F
                    ? lerp(0.0F, 1.0F, fraction * 2.0F)
                    : lerp(1.0F, 0.0F, (fraction - 0.5F) * 2.0F);
            }
        } else {
            const bool revealed = slot < frame.radar_reveal.size()
                && frame.radar_reveal[slot];
            if (!revealed) {
                continue;
            }
            if (slot < frame.radar_reveal_previous.size()
                && frame.radar_reveal_previous[slot]) {
                reveal = 2;
            } else if (reveal == 0) {
                // Newly revealed, which is what earns the taunt.
                reveal = 1;
            }
        }
        net::Vec3 position = player.position;
        if ((player.flags & net::PlayerState::FlagAltForm) == 0) {
            position.y += 0.75F;
        }
        AddLocatorInfo(state, position, LocatorIcon::Enemy, LocatorNeutral,
                       alpha);
    }
}

void PlayerHud::ProcessHudBounty(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                 const ModeHudFrame& frame,
                                 ModeHudState& state) {
    const int own_team = team_of(context, context.local_slot);
    // Carrying one: the marker points at where it has to be taken.
    const bool carrying = std::any_of(
        objectives.flags.begin(), objectives.flags.end(),
        [&context](const gameplay::FlagObjectiveState& flag) {
            return flag.carrier_slot == context.local_slot;
        });
    if (carrying) {
        for (const auto& flag : objectives.flags) {
            AddLocatorInfo(state, point_of(flag.base_position),
                           LocatorIcon::Node, LocatorGood);
        }
        return;
    }
    for (const auto& flag : objectives.flags) {
        HudBackend::Color color = LocatorNeutral;
        if (flag.carrier_slot != 0xff
            && (frame.frame_count & (4u * 2u)) != 0) {
            // A carried octolith blinks between white and whose it is.
            color = team_of(context, flag.carrier_slot) == own_team
                ? LocatorGood : LocatorHostile;
        }
        AddLocatorInfo(state, point_of(flag.position), LocatorIcon::Octolith,
                       color);
    }
}

void PlayerHud::ProcessHudCapture(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                  const ModeHudFrame& frame,
                                  ModeHudState& state) {
    const int own_team = team_of(context, context.local_slot);
    const bool carrying = std::any_of(
        objectives.flags.begin(), objectives.flags.end(),
        [&context](const gameplay::FlagObjectiveState& flag) {
            return flag.carrier_slot == context.local_slot;
        });
    for (const auto& flag : objectives.flags) {
        if (flag.carrier_slot == context.local_slot) {
            continue;
        }
        HudBackend::Color color = team_color(flag.team_id);
        if (flag.carrier_slot != 0xff
            && (frame.frame_count & (4u * 2u)) != 0) {
            color = team_of(context, flag.carrier_slot) == own_team
                ? LocatorGood : LocatorHostile;
        }
        AddLocatorInfo(state, point_of(flag.position), LocatorIcon::Octolith,
                       color);
        if (carrying && static_cast<int>(flag.team_id) == own_team) {
            // Carrying theirs, so our own base is where it goes.
            AddLocatorInfo(state, point_of(flag.base_position),
                           LocatorIcon::Node, LocatorGood);
        }
    }
}

void PlayerHud::ProcessHudDefender(
    const HudContext& context, const gameplay::ObjectiveState& objectives,
    ModeHudState& state) {
    if (context.state == nullptr) {
        return;
    }
    const int own_team = team_of(context, context.local_slot);
    for (const auto& node : objectives.nodes) {
        const int current = static_cast<int>(node.current_team);
        HudBackend::Color color;
        if (current == gameplay::NeutralObjectiveTeam) {
            color = LocatorNeutral;
        } else if (context.state->teams) {
            color = team_color(current);
        } else if (current == own_team) {
            color = LocatorGood;
        } else {
            color = LocatorHostile;
        }
        // The managed entity's Position is the middle of its volume.
        AddLocatorInfo(state, point_of(node.volume.center()),
                       LocatorIcon::Node, color);
    }
}

void PlayerHud::ProcessHudNodes(
    const HudContext& context, const gameplay::ObjectiveState& objectives,
    ModeHudState& state) {
    state.node_bonus_opponent = -1;
    state.main_node_bonus = false;
    state.team_node_counts.fill(0);
    state.queue_acquiring_node = false;
    state.clear_node_messages = false;
    if (context.state == nullptr) {
        return;
    }
    const int own_team = team_of(context, context.local_slot);
    bool show_bar = false;
    for (const auto& node : objectives.nodes) {
        const int current = static_cast<int>(node.current_team);
        const int occupying = static_cast<int>(node.occupying_team);
        // A node being taken blinks in the colour of whoever is taking it.
        const bool blinking = node.contested || node.progress > 0.0F;
        HudBackend::Color color;
        if (current == gameplay::NeutralObjectiveTeam) {
            if (!blinking) {
                color = LocatorNeutral;
            } else if (context.state->teams) {
                color = team_color(occupying);
            } else {
                color = occupying == own_team ? LocatorGood : LocatorHostile;
            }
        } else if (context.state->teams) {
            color = team_color(blinking ? occupying : current);
        } else if (current == own_team) {
            color = !blinking || occupying == own_team ? LocatorGood
                                                       : LocatorHostile;
        } else if (blinking && occupying == own_team) {
            color = LocatorGood;
        } else {
            color = LocatorHostile;
        }
        // The managed entity's Position is the middle of its volume.
        AddLocatorInfo(state, point_of(node.volume.center()),
                       LocatorIcon::Node, color);
        if (current != gameplay::NeutralObjectiveTeam
            && occupying == gameplay::NeutralObjectiveTeam
            && current >= 0
            && static_cast<std::size_t>(current)
                < state.team_node_counts.size()) {
            // Holding more than one node at once is worth a bonus, and the
            // HUD says which team is running one.
            const int count =
                ++state.team_node_counts[static_cast<std::size_t>(current)];
            if (count > 1) {
                if (current == own_team) {
                    state.main_node_bonus = true;
                } else if (state.node_bonus_opponent == -1
                           || count > state.team_node_counts[
                               static_cast<std::size_t>(
                                   state.node_bonus_opponent)]) {
                    state.node_bonus_opponent = current;
                }
            }
        }
        if (node.captured_by_slot == context.local_slot
            || (node.progress > 0.0F && occupying == own_team)) {
            show_bar = true;
            if (state.nodes_hud_state == 0) {
                state.queue_acquiring_node = true;
                state.nodes_progress_amount = 0;
                state.nodes_hud_state = 1;
            } else if (state.nodes_hud_state == 1) {
                // The bar fills over three hundred of the cartridge's
                // frames, in forty steps.
                state.nodes_progress_amount = static_cast<int>(std::lround(
                    lerp(0.0F, 40.0F, node.progress / (300.0F / 30.0F))));
            }
        }
    }
    if (!show_bar && state.nodes_hud_state != 0) {
        state.clear_node_messages = true;
        state.nodes_hud_state = 0;
    }
}

void PlayerHud::ProcessHudPrimeHunter(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                      const ModeHudFrame& frame,
                                      ModeHudState& state) {
    state.restart_prime_hunter_animation = false;
    const auto prime = objectives.prime_hunter;
    if (prime >= 0
        && static_cast<std::uint8_t>(prime) == context.local_slot) {
        if (!state.is_prime_hunter) {
            state.restart_prime_hunter_animation = true;
            state.prime_hunter_text_timer = 90.0F / 30.0F;
            state.is_prime_hunter = true;
        }
        if (state.prime_hunter_text_timer > 0.0F) {
            state.prime_hunter_text_timer -= frame.frame_time;
        }
        return;
    }
    state.is_prime_hunter = false;
    if (prime < 0) {
        return;
    }
    const auto slot = static_cast<std::uint8_t>(prime);
    if (context.session == nullptr || !context.session->has_player(slot)) {
        return;
    }
    // Everyone else gets a marker pointing at whoever is winning.
    const auto& player = context.session->player(slot);
    net::Vec3 position = player.position;
    if ((player.flags & net::PlayerState::FlagAltForm) == 0) {
        position.y += 0.75F;
    }
    AddLocatorInfo(state, position, LocatorIcon::Enemy, LocatorHostile);
}

bool PlayerHud::ProcessModeHud(const HudContext& context,
                               const gameplay::ObjectiveState& objectives,
                               const ModeHudFrame& frame,
                               ModeHudState& state) {
    state.locators.clear();
    int reveal = 0;
    switch (process_mode_hud(context.mode)) {
    case ModeHud::Survival:
        ProcessHudSurvival(context, frame, state, reveal);
        break;
    case ModeHud::Bounty:
        ProcessHudBounty(context, objectives, frame, state);
        break;
    case ModeHud::Capture:
        ProcessHudCapture(context, objectives, frame, state);
        break;
    case ModeHud::Defender:
        ProcessHudDefender(context, objectives, state);
        break;
    case ModeHud::Nodes:
        ProcessHudNodes(context, objectives, state);
        break;
    case ModeHud::PrimeHunter:
        ProcessHudPrimeHunter(context, objectives, frame, state);
        break;
    case ModeHud::None:
        break;
    }
    // One is "somebody just gave themselves away", which is the only case
    // the taunt fires on; two means they were already showing.
    return reveal == 1;
}

void PlayerHud::DrawOctolithInst(
    const HudContext& context, const gameplay::ObjectiveState& objectives,
    const ModeHudFrame& frame, const int icon_frame) {
    if (context.backend == nullptr || context.state == nullptr) {
        return;
    }
    const bool carrying = std::any_of(
        objectives.flags.begin(), objectives.flags.end(),
        [&context](const gameplay::FlagObjectiveState& flag) {
            return flag.carrier_slot == context.local_slot;
        });
    bool draw = false;
    float alpha = 1.0F;
    if (carrying) {
        // Carrying one: the icon flashes, so a player cannot miss it.
        draw = (frame.frame_count & (16u * 2u)) != 0;
    } else if (context.state->teams) {
        const int own_team = static_cast<int>(
            context.state->player_teams[context.local_slot]);
        for (const auto& flag : objectives.flags) {
            if (flag.carrier_slot != 0xff
                && static_cast<int>(
                    context.state->player_teams[flag.carrier_slot])
                    == own_team) {
                // A team mate has one: shown, but at half light, because it
                // is not this player who has to get it home.
                draw = true;
                alpha = 0.5F;
                break;
            }
        }
    }
    if (!draw) {
        return;
    }
    const auto& objects = objects_for(context);
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::Octolith, 0,
        static_cast<std::size_t>(std::max(0, icon_frame)), 0,
        static_cast<float>(objects.octolith_pos_x),
        static_cast<float>(objects.octolith_pos_y), 1.0F, alpha, nullptr));
}

void PlayerHud::DrawHudBattle(const HudContext& context) {
    DrawModeScore(context, BattleScoreMessageId,
                  FormatModeScore(context, context.local_slot));
}

void PlayerHud::DrawHudSurvival(const HudContext& context) {
    DrawModeScore(context, SurvivalScoreMessageId,
                  FormatModeScore(context, context.local_slot));
}

void PlayerHud::DrawHudBounty(const HudContext& context,
                              const gameplay::ObjectiveState& objectives,
                              const ModeHudFrame& frame) {
    DrawModeScore(context, BountyScoreMessageId,
                  FormatModeScore(context, context.local_slot));
    DrawOctolithInst(context, objectives, frame, 0);
}

void PlayerHud::DrawHudCapture(const HudContext& context,
                               const gameplay::ObjectiveState& objectives,
                               const ModeHudFrame& frame) {
    DrawModeScore(context, CaptureScoreMessageId,
                  FormatModeScore(context, context.local_slot));
    // Each side's octolith has its own frame.
    const int own_team = context.state == nullptr ? 0
        : static_cast<int>(context.state->player_teams[context.local_slot]);
    DrawOctolithInst(context, objectives, frame, own_team == 0 ? 4 : 3);
}

void PlayerHud::DrawHudDefender(const HudContext& context) {
    DrawModeScore(context, DefenderScoreMessageId,
                  FormatModeScore(context, context.local_slot));
}

void PlayerHud::DrawNodesBonuses(const HudContext& context,
                                 const ModeHudState& state) {
    if (context.backend == nullptr || context.state == nullptr) {
        return;
    }
    const auto& objects = objects_for(context);
    const std::string message = call(context.hud_message,
                                     NodesBonusMessageId);
    const int own_team = static_cast<int>(
        context.state->player_teams[context.local_slot]);
    if (state.main_node_bonus) {
        const std::size_t icon =
            context.state->teams && own_team == 0 ? 2u : 4u;
        static_cast<void>(context.backend->draw_hud_object(
            HudBackend::Object::Nodes, 0, icon, 0,
            static_cast<float>(objects.node_bonus_pos_x),
            static_cast<float>(objects.node_bonus_pos_y), 1.0F, 1.0F,
            nullptr));
        const std::string count = "x " + std::to_string(
            state.team_node_counts[static_cast<std::size_t>(
                std::clamp(own_team, 0, 3))]);
        static_cast<void>(DrawText2D(
            context, static_cast<float>(objects.node_bonus_pos_x + 12),
            static_cast<float>(objects.node_bonus_pos_y + 2), Align::Left, 0,
            count));
        static_cast<void>(DrawText2D(
            context, static_cast<float>(objects.node_bonus_pos_x),
            static_cast<float>(objects.node_bonus_pos_y + 10), Align::Left, 0,
            message));
    }
    // An opponent's bonus blinks: twelve frames on out of every sixteen.
    const float period = 16.0F / 30.0F;
    float past = std::fmod(context.elapsed_seconds, period);
    if (past < 0.0F) {
        past += period;
    }
    if (state.node_bonus_opponent != -1 && past < 12.0F / 30.0F) {
        const std::size_t icon =
            context.state->teams && state.node_bonus_opponent == 1 ? 4u : 2u;
        static_cast<void>(context.backend->draw_hud_object(
            HudBackend::Object::Nodes, 0, icon, 0,
            static_cast<float>(objects.enemy_bonus_pos_x),
            static_cast<float>(objects.enemy_bonus_pos_y), 1.0F, 1.0F,
            nullptr));
        const std::string count = "x " + std::to_string(
            state.team_node_counts[static_cast<std::size_t>(
                std::clamp(state.node_bonus_opponent, 0, 3))]);
        // Palette two is the red one: an opponent's bonus is a warning.
        static_cast<void>(DrawText2D(
            context, static_cast<float>(objects.enemy_bonus_pos_x + 12),
            static_cast<float>(objects.enemy_bonus_pos_y + 2), Align::Left, 2,
            count));
        static_cast<void>(DrawText2D(
            context, static_cast<float>(objects.enemy_bonus_pos_x),
            static_cast<float>(objects.enemy_bonus_pos_y + 10), Align::Left,
            2, message));
    }
}

void PlayerHud::DrawNodesIcons(const HudContext& context,
                               const gameplay::ObjectiveState& objectives) {
    if (context.backend == nullptr || context.state == nullptr) {
        return;
    }
    const auto& objects = objects_for(context);
    const auto count = objectives.nodes.size();
    // Four icons sit centred on the layout position; fewer are centred on
    // themselves instead.
    float start_x = 12.0F;
    if (count < 4) {
        start_x = static_cast<float>(16 * count) / 2.0F - 12.0F;
    }
    const int own_team = static_cast<int>(
        context.state->player_teams[context.local_slot]);
    float offset = 0.0F;
    for (const auto& node : objectives.nodes) {
        const int current = static_cast<int>(node.current_team);
        const int occupying = static_cast<int>(node.occupying_team);
        const bool blinking = node.contested || node.progress > 0.0F;
        // Zero is the unclaimed icon, two the hostile one and four the
        // friendly one -- the same three the locators use.
        std::size_t icon = 0;
        if (current == gameplay::NeutralObjectiveTeam) {
            if (blinking) {
                icon = context.state->teams
                    ? (occupying == 0 ? 2u : 4u)
                    : (occupying == own_team ? 4u : 2u);
            }
        } else if (context.state->teams) {
            icon = (blinking ? occupying : current) == 0 ? 2u : 4u;
        } else if (current == own_team) {
            icon = !blinking || occupying == own_team ? 4u : 2u;
        } else {
            icon = blinking && occupying == own_team ? 4u : 2u;
        }
        static_cast<void>(context.backend->draw_hud_object(
            HudBackend::Object::Nodes, 0, icon, 0,
            static_cast<float>(objects.node_icon_pos_x) + start_x - offset,
            static_cast<float>(objects.node_icon_pos_y - 8), 1.0F, 1.0F,
            nullptr));
        offset += 16.0F;
    }
    static_cast<void>(DrawText2D(
        context, static_cast<float>(objects.node_text_pos_x),
        static_cast<float>(objects.node_text_pos_y), Align::Center, 0,
        call(context.hud_message, NodesLabelMessageId)));
}

void PlayerHud::DrawHudNodes(const HudContext& context,
                             const gameplay::ObjectiveState& objectives,
                             const ModeHudFrame& frame,
                             const ModeHudState& state) {
    static_cast<void>(frame);
    DrawModeScore(context, NodesScoreMessageId,
                  FormatModeScore(context, context.local_slot));
    DrawNodesBonuses(context, state);
    DrawNodesIcons(context, objectives);
    if (state.nodes_hud_state != 1) {
        return;
    }
    // The progress bar takes the place of the "acquiring node" line once
    // that line has run its course.
    MphReadNative::Hud::Meter meter =
        MphReadNative::Hud::elements().node_progress_bar;
    meter.tank_amount = 40;
    meter.tank_count = 0;
    static_cast<void>(DrawMeter(context, HudBackend::Object::AmmoBar, 108.0F,
                                143.0F, state.nodes_progress_amount,
                                state.nodes_progress_amount, meter, 0,
                                false));
    static_cast<void>(DrawText2D(
        context, 128.0F, 133.0F, Align::Center, 0,
        call(context.hud_message, NodesProgressMessageId)));
}

void PlayerHud::DrawModeHud(const HudContext& context,
                            const gameplay::ObjectiveState& objectives,
                            const ModeHudFrame& frame,
                            const ModeHudState& state) {
    switch (context.mode) {
    case game::Mode::Battle:
    case game::Mode::BattleTeams:
        DrawHudBattle(context);
        break;
    case game::Mode::Survival:
    case game::Mode::SurvivalTeams:
        DrawHudSurvival(context);
        break;
    case game::Mode::Bounty:
    case game::Mode::BountyTeams:
        DrawHudBounty(context, objectives, frame);
        break;
    case game::Mode::Capture:
        DrawHudCapture(context, objectives, frame);
        break;
    case game::Mode::Defender:
    case game::Mode::DefenderTeams:
        DrawHudDefender(context);
        break;
    case game::Mode::Nodes:
    case game::Mode::NodesTeams:
        DrawHudNodes(context, objectives, frame, state);
        break;
    default:
        // Prime Hunter draws its own countdown, and the story mode's HUD is
        // the scan visor's, which is not this file's.
        break;
    }
}

void PlayerHud::DrawDoubleDamageHud(const HudContext& context,
                                    const PlayerHudState& hud,
                                    const float remaining,
                                    const float shift_x,
                                    const float shift_y) {
    if (context.backend == nullptr || remaining <= 0.0F) {
        return;
    }
    const auto& objects = objects_for(context);
    const float x = static_cast<float>(objects.dbl_dmg_pos_x) + shift_x;
    const float y = static_cast<float>(objects.dbl_dmg_pos_y) + shift_y;
    // The icon is drawn from its middle, which is sixteen units in.
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::DoubleDamage, 0,
        static_cast<std::size_t>(hud.double_damage_icon_frame()), 0,
        x - 16.0F, y - 16.0F, 1.0F, 0.5F, nullptr));
    if (hud.double_damage_text_timer() <= 0.0F) {
        return;
    }
    // The name types itself out at a character a frame.
    const float elapsed = 60.0F / 30.0F - hud.double_damage_text_timer();
    const int length = static_cast<int>(std::ceil(elapsed / (1.0F / 30.0F)));
    static_cast<void>(DrawText2D(
        context, x + static_cast<float>(objects.dbl_dmg_text_pos_x),
        y + static_cast<float>(objects.dbl_dmg_text_pos_y),
        objects.dbl_dmg_align, 0,
        call(context.hud_message, DoubleDamageMessageId), nullptr, 1.0F,
        1.0F, length));
}

void PlayerHud::DrawCloakHud(const HudContext& context,
                             const PlayerHudState& hud, const bool cloaking,
                             const float shift_x, const float shift_y) {
    if (context.backend == nullptr || !cloaking) {
        return;
    }
    const auto& objects = objects_for(context);
    const float x = static_cast<float>(objects.cloak_pos_x) + shift_x;
    const float y = static_cast<float>(objects.cloak_pos_y) + shift_y;
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::Cloak, 0, 0, 0, x - 16.0F, y - 16.0F, 1.0F, 0.5F,
        nullptr));
    if (hud.cloak_text_timer() <= 0.0F) {
        return;
    }
    const float elapsed = 45.0F / 30.0F - hud.cloak_text_timer();
    const int length = static_cast<int>(std::ceil(elapsed / (1.0F / 30.0F)));
    static_cast<void>(DrawText2D(
        context, x + static_cast<float>(objects.cloak_text_pos_x),
        y + static_cast<float>(objects.cloak_text_pos_y),
        objects.cloak_align, 0, call(context.hud_message, CloakMessageId),
        nullptr, 1.0F, 1.0F, length));
}

bool PlayerHud::DrawTargetHealthbar(const HudContext& context,
                                    const TargetHealth& target,
                                    const float shift_x,
                                    const float shift_y) {
    if (context.backend == nullptr || target.max <= 0) {
        return false;
    }
    const auto& objects = objects_for(context);
    // Palette two is the red one.
    const std::size_t palette = target.current > target.low_health ? 0u : 2u;
    MphReadNative::Hud::Meter meter =
        MphReadNative::Hud::elements().enemy_healthbar;
    meter.tank_amount = target.max;
    meter.tank_count = 0;
    // The bar's length is the sub-healthbar's and does not follow the
    // hunter's own values, which is why it is taken from index zero.
    meter.length = MphReadNative::Hud::elements().sub_healthbars[0].length;
    static_cast<void>(DrawMeter(
        context, HudBackend::Object::HealthBarSub,
        static_cast<float>(objects.enemy_health_pos_x) + shift_x,
        static_cast<float>(objects.enemy_health_pos_y) + shift_y,
        target.max, target.current, meter, palette, false));
    if (!target.text.empty()) {
        static_cast<void>(DrawText2D(
            context,
            static_cast<float>(objects.enemy_health_text_pos_x) + shift_x,
            static_cast<float>(objects.enemy_health_text_pos_y) + shift_y,
            Align::Center, palette, target.text));
    }
    // False means the target is dead, which is how the caller knows to stop
    // drawing it.
    return target.current > 0;
}

void PlayerHud::DrawOpponent(const HudContext& context,
                             const PlayerHudState& hud,
                             const std::uint16_t energy_tank,
                             const float shift_x, const float shift_y) {
    const int slot = hud.opponent_index();
    if (slot < 0 || hud.opponent_healthbar_timer() <= 0.0F
        || context.backend == nullptr || context.session == nullptr
        || energy_tank == 0
        || !context.session->has_player(static_cast<std::uint8_t>(slot))) {
        return;
    }
    const auto& opponent = context.session->player(
        static_cast<std::uint8_t>(slot));
    float x = 93.0F + shift_x;
    float y = 182.0F + shift_y;
    const std::string nickname = context.nickname
        ? context.nickname(static_cast<std::uint8_t>(slot)) : std::string{};
    if (!nickname.empty()) {
        static_cast<void>(DrawText2D(context, x, y, Align::Center, 0,
                                     nickname));
    }
    const std::size_t hunter = std::min<std::size_t>(
        context.session->player_hunter(static_cast<std::uint8_t>(slot)), 7);
    static_cast<void>(context.backend->draw_hud_object(
        HudBackend::Object::HunterPortrait, hunter, 0, 0, x - 16.0F,
        y - 33.0F, 1.0F, 1.0F, nullptr));
    x += 18.0F;
    y -= 26.0F;
    // Two bars: the first tank, and everything past it.  A hunter with no
    // tanks shows an empty second bar rather than none at all.
    const int health = static_cast<int>(opponent.health);
    const int remaining = health >= static_cast<int>(energy_tank)
        ? health - static_cast<int>(energy_tank) : 0;
    MphReadNative::Hud::Meter meter =
        MphReadNative::Hud::elements().enemy_healthbar;
    meter.tank_amount = static_cast<int>(energy_tank);
    meter.tank_count = static_cast<int>(
        context.session->inventory(static_cast<std::uint8_t>(slot))
            .health_max / energy_tank);
    meter.length = 72;
    static_cast<void>(DrawMeter(context, HudBackend::Object::HealthBarSub, x,
                                y, static_cast<int>(energy_tank) - 1, health,
                                meter, 0, false));
    static_cast<void>(DrawMeter(context, HudBackend::Object::HealthBarSub, x,
                                y + 5.0F,
                                static_cast<int>(energy_tank) - 1, remaining,
                                meter, 0, false));
    static_cast<void>(DrawText2D(
        context, x + 5.0F, y + 14.0F, Align::Left, 0,
        FormatModeScore(context, static_cast<std::uint8_t>(slot))));
}

PlayerHud::HudFont PlayerHud::SetUpFont(const char first_character,
                                         const bool japanese) noexcept {
    const auto lead = static_cast<unsigned char>(first_character);
    return japanese && (lead & 0xA0u) == 0xA0u ? HudFont::Kanji
                                               : HudFont::Normal;
}

void PlayerHud::DrawHudPrimeHunter(const HudContext& context,
                                   const ModeHudState& state,
                                   const float shift_x,
                                   const float shift_y) {
    if (context.backend != nullptr && state.is_prime_hunter) {
        const auto& objects = objects_for(context);
        const float x = static_cast<float>(objects.prime_pos_x) + shift_x;
        const float y = static_cast<float>(objects.prime_pos_y) + shift_y;
        static_cast<void>(context.backend->draw_hud_object(
            HudBackend::Object::PrimeHunter, 0, 0, 0, x - 16.0F, y - 16.0F,
            1.0F, 1.0F, nullptr));
        if (state.prime_hunter_text_timer > 0.0F) {
            // Three seconds, typed out a character a frame.
            const float elapsed =
                90.0F / 30.0F - state.prime_hunter_text_timer;
            const int length =
                static_cast<int>(std::ceil(elapsed / (1.0F / 30.0F)));
            static_cast<void>(DrawText2D(
                context, x + static_cast<float>(objects.prime_text_pos_x),
                y + static_cast<float>(objects.prime_text_pos_y),
                objects.prime_align, 0,
                call(context.hud_message, PrimeHunterMessageId), nullptr,
                1.0F, 1.0F, length));
        }
    }
    DrawModeScore(context, PrimeTimeMessageId,
                  FormatModeScore(context, context.local_slot));
}

void PlayerHud::DrawBoostBombs(const HudContext& context,
                               const AltFormReadout& readout,
                               const float y_offset) {
    if (context.backend == nullptr) {
        return;
    }
    if (readout.has_bombs && !readout.is_kanden) {
        // Three bombs, right to left, lit ones first.
        float x = 244.0F;
        for (int i = 3; i > 0; --i) {
            const std::size_t frame = readout.bomb_ammo < i ? 1u : 0u;
            static_cast<void>(context.backend->draw_hud_object(
                HudBackend::Object::Bomb, 0, frame, 0, x - 8.0F, y_offset,
                1.0F, 1.0F, nullptr));
            x -= 14.0F;
        }
        static_cast<void>(DrawText2D(
            context, 230.0F, y_offset + 18.0F, Align::Center, 0,
            call(context.hud_message, BombsMessageId)));
    }
    if (readout.has_boost) {
        static_cast<void>(context.backend->draw_hud_object(
            HudBackend::Object::Boost, 0, readout.boost_ready ? 0u : 1u, 0,
            29.0F - 8.0F, y_offset - 16.0F, 1.0F, 1.0F, nullptr));
        static_cast<void>(DrawText2D(
            context, 29.0F, y_offset + 18.0F, Align::Center, 0,
            call(context.hud_message, BoostMessageId)));
    }
}

void PlayerHud::DrawLocatorIcons(
    const HudContext& context, const ModeHudState& state,
    const float width, const float height,
    const std::function<ProjectedPoint(const net::Vec3&)>& project) {
    if (context.backend == nullptr || !project) {
        return;
    }
    for (const auto& info : state.locators) {
        DrawLocatorIcon(context, info, project(info.position), width, height);
    }
}

void PlayerHud::DrawLocatorIcon(const HudContext& context,
                                const LocatorInfo& info,
                                const ProjectedPoint& point,
                                const float width, const float height) {
    if (context.backend == nullptr) {
        return;
    }
    {
        const LocatorPlacement placement = PlaceLocatorIcon(
            point.view, point.x, point.y, width, height);
        // A marker off the screen is replaced by an arrow pointing at it,
        // rather than being clamped and left looking like the thing is at
        // the edge of the room.
        const LocatorIcon icon = placement.arrow ? LocatorIcon::Arrow
                                                 : info.icon;
        HudBackend::Object object = HudBackend::Object::NodeLocator;
        switch (icon) {
        case LocatorIcon::Octolith:
            object = HudBackend::Object::OctolithLocator;
            break;
        case LocatorIcon::Enemy:
            object = HudBackend::Object::PlayerLocator;
            break;
        case LocatorIcon::Arrow:
            object = HudBackend::Object::ArrowLocator;
            break;
        case LocatorIcon::Node:
            break;
        }
        // A locator is a model rather than a sprite, because the arrow has
        // to point somewhere -- which is what separates this from every
        // other thing the HUD draws.
        context.backend->draw_icon_model(object, placement.x, placement.y,
                                         placement.angle, info.color,
                                         info.alpha);
    }
}

} // namespace fruityprime::players
