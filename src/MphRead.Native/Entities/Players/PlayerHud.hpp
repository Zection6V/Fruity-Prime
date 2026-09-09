#pragma once

#include "GameState.hpp"
#include "Entities/gameplay.hpp"
#include "../../HUD/hud.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace fruityprime::players {

// Native counterpart of PlayerHud.cs.  Keeping this adapter in the same
// source folder makes the partial PlayerEntity boundary explicit while the
// HUD renderer remains toolkit-neutral.

// Scene.DrawHudObject and Scene.DrawHudFilterModel, which is everything the
// managed PlayerHud needs a renderer for.  The seam is the same shape as
// ScenePassBackend: the layout stays here, in the counterpart of the file it
// came from, and only the pixels are somebody else's problem.
//
// Every coordinate crossing it is in the DS's own 256x192 space, because that
// is the space PlayerHud.cs is written in.
class HudBackend {
public:
    virtual ~HudBackend() = default;

    // The HUD objects PlayerHud draws with.  HunterPortrait is an array --
    // one per hunter -- so it takes a variant; the rest ignore it.
    enum class Object : std::uint8_t {
        Font,
        FontMask,
        HealthBar,
        HealthBarSub,
        AmmoBar,
        WeaponIcon,
        Stars,
        HunterPortrait,
    };

    // HudObjectInstance.SetData's explicit-colour form: every lit pixel
    // becomes this rather than taking the palette entry.
    struct Color {
        float red = 1.0F;
        float green = 1.0F;
        float blue = 1.0F;
    };

    [[nodiscard]] virtual bool draw_hud_object(
        Object object, std::size_t variant, std::size_t frame,
        std::size_t palette, float x, float y, float scale, float alpha,
        const Color* color) = 0;

    virtual void draw_hud_filter_model(float alpha) = 0;
};

// HudInfo.Align, which hud.hpp already carries.
using Align = MphReadNative::Hud::Align;

// What the managed PlayerHud reads off GameState, the scene and the player it
// belongs to.  Those are statics and fields there; here they are handed in,
// which is the one shape change the language forces.
struct HudContext {
    HudBackend* backend = nullptr;
    const gameplay::Session* session = nullptr;
    const game::State* state = nullptr;
    std::uint8_t local_slot = 0;
    std::uint8_t hunter = 0;
    // Scene.ElapsedTime, and the time the intro camera sequence has run for.
    float elapsed_seconds = 0.0F;
    float intro_seconds = 0.0F;
    float frames_per_second = 0.0F;
    float match_time_remaining = -1.0F;
    game::Mode mode = game::Mode::Battle;
    bool match_over = false;
    bool networked = false;
    // Strings.GetHudMessage and GetMessage('S', ...), which need the string
    // tables the host loaded.
    std::function<std::string(int)> hud_message;
    std::function<std::string(int)> rules_message;
    std::function<std::string(std::uint8_t)> nickname;
    std::function<int(std::uint8_t)> ping;
};

class PlayerHud final {
public:
    [[nodiscard]] static MphReadNative::Hud::PlayerState build(
        const gameplay::Session& session, std::uint8_t slot);

    // What UpdateHud has to know about the frame.
    struct HudFrame {
        // GameState.MenuPause: the pause menu is up.
        bool menu_pause = false;
        // GameState.DialogPause: a story dialog has stopped time.
        bool dialog_pause = false;
        bool single_player = false;
        // PlayerFlags1.WeaponMenuOpen
        bool weapon_menu_open = false;
    };

    // What one UpdateHud pass decided.  The managed method drives a dozen
    // sub-updates directly; the decision it makes first is the one worth
    // keeping separable, because two of the three branches skip almost
    // everything.
    struct HudUpdate {
        // The pause background is drawn on the visor layer, at three quarters
        // alpha and shifted a third of a screen up, with the helmet front left
        // alone on top of it.
        bool draw_pause_background = false;
        static constexpr float PauseAlpha = 0.75F;
        static constexpr float PauseShiftY = -1.0F / 3.0F;
        // False while paused or in a dialog: the readouts are frozen where
        // they were rather than continuing to animate behind the menu.
        bool update_readouts = false;
        // Dialogs are only updated in the story.
        bool update_dialogs = false;
        bool update_weapon_menu = false;
    };

    // PlayerHud.UpdateHud
    [[nodiscard]] static HudUpdate update_hud(const HudFrame& frame) noexcept;

    // PlayerHud.ProcessModeHud: which mode-specific HUD runs.  Every mode
    // clears the locator list and processes the opponent marker first,
    // whatever it dispatches to afterwards.
    enum class ModeHud : std::uint8_t {
        None,
        Survival,
        Bounty,
        Capture,
        Defender,
        Nodes,
        PrimeHunter,
    };
    [[nodiscard]] static ModeHud process_mode_hud(game::Mode mode) noexcept;

    // What DrawHudObjects has to know about the frame.
    struct DrawFrame {
        // Mods.ThumbnailMode.Active: a thumbnail is being captured, so no HUD
        // at all.
        bool thumbnail_mode = false;
        // Mods.RenderOptions.ShowFps
        bool show_fps = false;
        // Mods.SpectatorMode.FreeCamera: watching the map rather than a
        // player.
        bool free_camera = false;
        bool menu_pause = false;
        bool show_scoreboard = false;
        game::MatchState match_state = game::MatchState::InProgress;
    };

    // What one DrawHudObjects pass draws.  The ordering here is the point: the
    // frame counter and the chat log are drawn before the pause and spectator
    // gates, because a switch with no visible feedback and a message you
    // cannot read while watching are both worse than a slightly busy screen.
    struct HudDraw {
        bool fps = false;
        bool chat = false;
        bool match_time = false;
        bool scoreboard = false;
        bool game_over_text = false;
        // The player's own readouts: energy, ammo, weapon, radar.
        bool player_hud = false;
    };
    [[nodiscard]] static HudDraw draw_hud_objects(
        const DrawFrame& frame) noexcept;

    // ---- the drawing half of PlayerHud.cs -------------------------------
    // Every one of these is the managed method of the same name, in the same
    // coordinate space and the same order.

    // PlayerHud.DrawText2D.  Returns where the run ended so a caller can hang
    // another off it, which is what DrawFps does.
    static float DrawText2D(const HudContext& context, float x, float y,
                            Align align, std::size_t palette,
                            std::string_view text,
                            const HudBackend::Color* color = nullptr,
                            float alpha = 1.0F, float scale = 1.0F,
                            int max_length = -1);

    // PlayerHud.FormatTime and FormatModeScore.
    [[nodiscard]] static std::string FormatTime(float seconds);
    [[nodiscard]] static std::string FormatModeScore(
        const HudContext& context, std::uint8_t slot);

    // PlayerHud.DrawMatchTime, DrawScoreboard and DrawScoreboardPlayer.
    static void DrawMatchTime(const HudContext& context);
    static void DrawScoreboard(const HudContext& context);
    static void DrawScoreboardPlayer(const HudContext& context, float x,
                                     float y, const HudBackend::Color& color,
                                     std::uint8_t slot, std::size_t hunter);

    // PlayerHud.DrawFps.
    static void DrawFps(const HudContext& context);

    // PlayerHud.DrawModeRules, and the line PlayerProcess queues underneath
    // it while the player is waiting to spawn.
    static void DrawModeRules(const HudContext& context);
    static void DrawRespawnPrompt(const HudContext& context);


    // PlayerHud.DrawMeter, DrawHealthbars, DrawAmmoBar and DrawModeScore.
    // Returns whether the meter drew anything, which is what tells the host
    // a cartridge-art HUD is available at all.
    static bool DrawMeter(const HudContext& context,
                          HudBackend::Object object, float x, float y,
                          int base_amount, int current_amount,
                          const MphReadNative::Hud::Meter& meter,
                          std::size_t palette, bool draw_text,
                          float alpha = 1.0F);
    static bool DrawHealthbars(const HudContext& context,
                               const MphReadNative::Hud::PlayerState& player,
                               float healthbar_y_offset);
    static void DrawAmmoBar(const HudContext& context,
                            const MphReadNative::Hud::PlayerState& player,
                            const gameplay::InventoryState& inventory);
    static void DrawModeScore(const HudContext& context, int message_id,
                              const std::string& text);
    // PlayerHud's _healthbarYOffset, eased half a unit a frame towards the
    // helmet's own resting offset.
    static float UpdateHealthbarOffset(const HudContext& context,
                                       float current, bool alt_form) noexcept;

    // Strings.GetHudMessage(219): "GAME OVER".
    static constexpr int GameOverMessageId = 219;
};

} // namespace fruityprime::players
