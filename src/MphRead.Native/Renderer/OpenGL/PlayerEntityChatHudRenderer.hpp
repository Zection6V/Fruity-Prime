#pragma once

#include <cstdint>
#include <string_view>

namespace fruityprime::renderer::opengl {

// Native rendering adapter for PlayerEntityChatHud.ModDrawChat. Layout and
// clipping stay in the C# counterpart; this function only maps one already
// selected glyph run to the active OpenGL HUD target.
void draw_chat_text(std::u16string_view text, float left, float top,
                    float aspect, int viewport_width, int viewport_height,
                    float alpha, std::uint8_t red, std::uint8_t green,
                    std::uint8_t blue);

} // namespace fruityprime::renderer::opengl
