#pragma once

#include "Mods/Chat/ChatBox.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::input {
class State;
}

namespace fruityprime::chat::player_entity_chat_hud {

// The fields correspond to PlayerEntityChatHud.cs's _chatInst and
// _chatVisible. They belong to one PlayerEntity, not to the process-wide
// ChatBox or to the native window host.
struct State {
    std::array<std::array<std::uint8_t, 4>, 2> palette{};
    bool palette_ready = false;
    bool character_ready = false;
    bool enabled = false;
    std::vector<VisibleChatLine> visible;
};

using DrawText = void (*)(std::u16string_view text, float left, float top,
                          float aspect, int viewport_width,
                          int viewport_height, float alpha,
                          std::uint8_t red, std::uint8_t green,
                          std::uint8_t blue);

// These values are the managed PlayerEntityChatHud constants. Coordinates
// remain in the game's 256x192 HUD space until the renderer maps them.
inline constexpr float Scale = 0.45F;
inline constexpr float LineHeight = 7.0F * Scale + 1.2F;
inline constexpr float Top = 3.0F;
inline constexpr float Bottom = Top + 4.0F * LineHeight;

[[nodiscard]] float aspect_fix(int width, int height) noexcept;
[[nodiscard]] float margin(bool android) noexcept;
[[nodiscard]] float clearance(bool available, float position_y) noexcept;

// ChatBox stores the network representation as UTF-8. PlayerEntityChatHud.cs
// operates on .NET UTF-16 code units, so the renderer converts at this seam
// before measuring, clipping, or drawing text.
[[nodiscard]] std::u16string utf8_to_utf16(std::string_view text);
[[nodiscard]] float width(std::u16string_view text, float aspect) noexcept;
[[nodiscard]] float room(float aspect, float used,
                         bool android = false) noexcept;
[[nodiscard]] std::u16string fit(std::u16string_view text, float aspect,
                                 float used, bool android = false);
[[nodiscard]] std::u16string tail(std::u16string_view text, float aspect,
                                  float used, bool android = false);

// Native's OpenGL backend does not have HudObjectInstance, but it still keeps
// the managed lazy palette/character initialization boundary explicit for one
// PlayerEntity instance.
void ensure_renderer(State& state) noexcept;

// Native entry-point glue calls this one method for ModDrawChat. The chat
// layout, text conversion, clipping, and glyph drawing all stay in this
// PlayerEntityChatHud counterpart rather than in the native WinMain host.
void draw(State& state, int viewport_width, int viewport_height,
          DrawText draw_text);

// Native input state is the adapter behind the four nullable managed input
// snapshots. Clearing it is the native equivalent of ModForgetInputDeltas.
void forget_input_deltas(input::State& state) noexcept;

} // namespace fruityprime::chat::player_entity_chat_hud
