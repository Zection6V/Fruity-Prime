// Native counterpart of src/MphRead/Entities/Players/PlayerHud.cs.
#include "PlayerHud.hpp"

#include "HUD/hud.hpp"
#include "Mods/Network/player_entity_net_hud.hpp"
#include "Strings.hpp"
#include "Metadata/metadata.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::players {
namespace {

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
// hunter portrait.
constexpr float ScoreStartSpace = 13.0F;
constexpr float ScorePlayerSpace = 28.0F;
constexpr float ScoreMinPlayerSpace = 19.0F;

[[nodiscard]] float scoreboard_row_space(const HudContext& context) {
    const int rows = static_cast<int>(context.state->active_players);
    if (rows <= 4) {
        return ScorePlayerSpace;
    }
    const float available = 168.0F - ScoreStartSpace;
    return std::max(ScoreMinPlayerSpace,
                    available / static_cast<float>(rows));
}

[[nodiscard]] float scoreboard_height(const HudContext& context) {
    return ScoreStartSpace
        + scoreboard_row_space(context)
            * static_cast<float>(context.state->active_players);
}

[[nodiscard]] std::string call(
    const std::function<std::string(int)>& lookup, int id) {
    return lookup ? lookup(id) : std::string{};
}

} // namespace

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
    const float row_space = scoreboard_row_space(context);
    float y = 104.0F - scoreboard_height(context) / 2.0F;
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
    float y = static_cast<float>(objects.score_pos_y);
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

} // namespace fruityprime::players
