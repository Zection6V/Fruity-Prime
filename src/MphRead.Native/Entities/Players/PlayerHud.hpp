#pragma once

#include "GameState.hpp"
#include "Entities/gameplay.hpp"
#include "../../HUD/hud.hpp"
#include "Mods/Chat/PlayerEntityChatHud.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <functional>
#include <string>
#include <string_view>

namespace fruityprime::players {

class PlayerEntity;

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

    // PlayerHud.DrawHudObjects calls PlayerEntity.ModDrawChat before its
    // pause, spectator, and intro branches. The host supplies only the
    // native glyph backend; chat ownership remains on PlayerEntity.
    static void DrawChat(
        PlayerEntity& player, int viewport_width, int viewport_height,
        fruityprime::chat::player_entity_chat_hud::DrawText draw_text);

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

    // PlayerHud's scoreboard geometry.  The stock 28 units a row fits
    // the four players a DS match could hold; with more the list runs off
    // both ends of a 192-unit screen, so it tightens to whatever fits and
    // stops at the height of a hunter icon.
    static constexpr float ScoreStartSpace = 13.0F;
    static constexpr float ScoreTeamHeaderSpace = 4.0F;
    static constexpr float ScoreTeamLineSpace = 18.0F;
    static constexpr float ScorePlayerSpace = 28.0F;
    static constexpr float ScoreMinPlayerSpace = 19.0F;

    // PlayerHud.GetScoreboardRowSpace and GetScoreboardHeight.
    [[nodiscard]] static float GetScoreboardRowSpace(
        const HudContext& context);
    [[nodiscard]] static float GetScoreboardHeight(const HudContext& context);

    // PlayerHud.DrawEscapeTime.  The escape timer counts hundredths, which
    // is why it has a format of its own rather than sharing FormatTime.
    static void DrawEscapeTime(const HudContext& context, float seconds,
                               float shift_x, float shift_y);

    // PlayerHud.LocatorInfo: one marker the mode HUD asked for this frame.
    // The icon is named rather than carried as a model, because which model
    // draws it is the renderer's half of the seam.
    enum class LocatorIcon : std::uint8_t {
        Node,
        Octolith,
        Enemy,
        Arrow
    };

    struct LocatorInfo {
        net::Vec3 position;
        LocatorIcon icon = LocatorIcon::Node;
        HudBackend::Color color{};
        float alpha = 1.0F;
    };

    // Where DrawLocatorIcon put a marker, in 0..1 of the viewport.  A
    // marker outside the box is pinned to its edge and drawn as an arrow
    // pointing at where the thing actually is.
    struct LocatorPlacement {
        float x = 0.0F;
        float y = 0.0F;
        float angle = 0.0F;
        bool arrow = false;
    };

    // PlayerHud.DrawLocatorIcon's geometry.  `view` is the position through
    // the view matrix and `projected` is its screen position, which only
    // means anything when the point is actually in front of the camera --
    // view.z < -1, which is what the cartridge tests.
    [[nodiscard]] static LocatorPlacement PlaceLocatorIcon(
        const net::Vec3& view, float projected_x, float projected_y,
        float width, float height) noexcept;
    // What the mode HUDs keep between frames, and what one pass of them
    // produced.  The managed class holds all of this as fields on the
    // player; a struct is the same thing with the ownership written down.
    struct ModeHudState {
        // PlayerHud._locatorInfo, rebuilt every frame.
        std::vector<LocatorInfo> locators;
        // Nodes: which opponent team is running a multi-node bonus, and
        // whether this player's own team is.
        int node_bonus_opponent = -1;
        bool main_node_bonus = false;
        std::array<int, 4> team_node_counts{};
        // The acquiring-node message and the bar under it.
        int nodes_hud_state = 0;
        int nodes_progress_amount = 0;
        // Prime Hunter: whether this player is it, and how long the
        // announcement stays up.
        bool is_prime_hunter = false;
        float prime_hunter_text_timer = 0.0F;
        // Set when ProcessHudNodes wants the "acquiring node" line queued,
        // or the node messages cleared: queueing is the caller's, because
        // the queue is the caller's.
        bool queue_acquiring_node = false;
        bool clear_node_messages = false;
        // Set when ProcessHudPrimeHunter wants the prime-hunter halo
        // animation restarted.
        bool restart_prime_hunter_animation = false;
    };

    // What ProcessModeHud has to know about the frame and the players in it.
    struct ModeHudFrame {
        // GameState.RadarPlayers: every opponent shows on the radar, fading
        // in and out rather than only when they fire.
        bool radar_players = false;
        // PlayerFlags2.RadarReveal and RadarRevealPrevious, per slot.  They
        // are player state this head does not replicate, so they are handed
        // in rather than guessed: an empty span means nobody is revealed.
        std::span<const bool> radar_reveal;
        std::span<const bool> radar_reveal_previous;
        // Scene.FrameCount, which the flag and node colours blink on.
        std::uint64_t frame_count = 0;
        float frame_time = 1.0F / 60.0F;
        // How long each opponent has been revealed, for the fade.
        std::span<const float> reveal_elapsed;
    };

    // PlayerHud.ProcessModeHud and the six ProcessHud* methods under it.
    // Returns whether the "COWARD DETECTED" line should be queued, which
    // ProcessHudSurvival decides and the caller acts on.
    static bool ProcessModeHud(const HudContext& context,
                               const gameplay::ObjectiveState& objectives,
                               const ModeHudFrame& frame,
                               ModeHudState& state);
    static void ProcessHudSurvival(const HudContext& context,
                                   const ModeHudFrame& frame,
                                   ModeHudState& state, int& reveal);
    static void ProcessHudBounty(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                 const ModeHudFrame& frame,
                                 ModeHudState& state);
    static void ProcessHudCapture(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                  const ModeHudFrame& frame,
                                  ModeHudState& state);
    static void ProcessHudDefender(
        const HudContext& context,
        const gameplay::ObjectiveState& objectives, ModeHudState& state);
    static void ProcessHudNodes(
        const HudContext& context,
        const gameplay::ObjectiveState& objectives, ModeHudState& state);
    static void ProcessHudPrimeHunter(const HudContext& context,
                   const gameplay::ObjectiveState& objectives,
                                      const ModeHudFrame& frame,
                                      ModeHudState& state);

    // PlayerHud.AddLocatorInfo and DrawLocatorIcons' list half.
    static void AddLocatorInfo(ModeHudState& state, const net::Vec3& position,
                               LocatorIcon icon,
                               const HudBackend::Color& color,
                               float alpha = 1.0F);

    // Strings.GetHudMessage(234): "COWARD DETECTED!"; (205): "acquiring
    // node".  The categories are the cartridge's own message masks.
    static constexpr int CowardMessageId = 234;
    static constexpr int AcquiringNodeMessageId = 205;
    static constexpr int NodeMessageMask = 16;


    // Strings.GetHudMessage(219): "GAME OVER".
    static constexpr int GameOverMessageId = 219;
};

// PlayerHud's private HudMessage.
struct HudMessage {
    float x = 0.0F;
    float y = 0.0F;
    float font_size = 0.0F;
    HudBackend::Color color{};
    float lifetime = 0.0F;
    float alpha = 0.0F;
    std::uint8_t category = 0;
    int max_width = 0;
    Align align = Align::Left;
    // The managed message holds a fixed 256-character buffer; the wrapping
    // is what matters and a string carries that unchanged.
    std::string text;
    bool dialog_hide = false;
};

class HudMessageQueue final {
public:
    // The cartridge keeps exactly twenty and reuses whichever has the least
    // life left, so a message is never refused and never allocates.
    static constexpr std::size_t Capacity = 20;

    // ColorRgba(0x3FEF), which every convenience overload passes.
    static constexpr HudBackend::Color DefaultColor{
        15.0F / 31.0F, 31.0F / 31.0F, 15.0F / 31.0F};

    // What DrawQueuedHudMessages reads off GameState and the scene.
    struct DrawFrame {
        bool menu_pause = false;
        bool dialog_pause = false;
        std::uint64_t frame_count = 0;
    };

    // PlayerHud.WrapText: copy `text` into `destination`, breaking it at
    // spaces so no line measures wider than `max_width`, and return how many
    // lines that took.  A line with no space to break at breaks after the
    // character that overflowed.
    [[nodiscard]] static int WrapText(std::string_view text, int max_width,
                                      std::string& destination,
                                      bool japanese = false);

    // PlayerHud.QueueHudMessage, in its three shapes.  The two short ones
    // are the managed convenience overloads; the message-id forms belong to
    // the caller, which is the only side that has the string table.
    void QueueHudMessage(float x, float y, float duration,
                         std::uint8_t category, std::string_view text,
                         bool dialog_hide = false);
    void QueueHudMessage(float x, float y, int max_width, float duration,
                         std::uint8_t category, std::string_view text,
                         bool dialog_hide = false);
    void QueueHudMessage(float x, float y, Align align, int max_width,
                         float font_size, const HudBackend::Color& color,
                         float alpha, float duration, std::uint8_t category,
                         std::string_view text, bool dialog_hide = false);

    // PlayerHud.ClearHudMessage and IsHudMessageQueued.
    void ClearHudMessage(int mask) noexcept;
    [[nodiscard]] bool IsHudMessageQueued(int mask) const noexcept;

    // PlayerHud.ProcessHudMessageQueue and DrawQueuedHudMessages.
    void ProcessHudMessageQueue(float frame_time) noexcept;
    void DrawQueuedHudMessages(const HudContext& context,
                               const DrawFrame& frame) const;

    [[nodiscard]] const std::array<HudMessage, Capacity>&
    messages() const noexcept {
        return messages_;
    }

    // Scene.Language: which font WrapText measures with.
    void set_japanese(bool value) noexcept { japanese_ = value; }

private:
    std::array<HudMessage, Capacity> messages_{};
    bool japanese_ = false;
};

} // namespace fruityprime::players
