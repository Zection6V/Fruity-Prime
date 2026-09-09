#pragma once

#include <string>
#include <string_view>

namespace fruityprime::chat::hud {

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
// the managed lazy palette/character initialization boundary explicit.
void ensure_renderer() noexcept;

} // namespace fruityprime::chat::hud
