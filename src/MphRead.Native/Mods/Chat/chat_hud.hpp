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

[[nodiscard]] float width(std::string_view text, float aspect) noexcept;
[[nodiscard]] float room(float aspect, float used) noexcept;
[[nodiscard]] std::string fit(std::string_view text, float aspect,
                              float used);
[[nodiscard]] std::string tail(std::string_view text, float aspect,
                               float used);

} // namespace fruityprime::chat::hud
