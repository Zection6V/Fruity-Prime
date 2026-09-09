#include "Renderer/OpenGL/PlayerEntityChatHudRenderer.hpp"

#include "Mods/Chat/chat_font.hpp"
#include "Mods/Chat/PlayerEntityChatHud.hpp"

#ifdef _WIN32
#include <GL/gl.h>
#endif

namespace fruityprime::renderer::opengl {

void draw_chat_text(const std::u16string_view text, const float left,
                    const float top, const float aspect,
                    const int viewport_width, const int viewport_height,
                    const float alpha, const std::uint8_t red,
                    const std::uint8_t green, const std::uint8_t blue) {
#ifdef _WIN32
    if (viewport_width <= 0 || viewport_height <= 0
        || chat::player_entity_chat_hud::Scale <= 0.0F) {
        return;
    }
    const float pixel_scale = static_cast<float>(viewport_height) / 192.0F
        * chat::player_entity_chat_hud::Scale;
    const float horizontal_scale = static_cast<float>(viewport_width) / 256.0F
        * aspect * chat::player_entity_chat_hud::Scale;
    float cursor = left / 256.0F * static_cast<float>(viewport_width);
    const float screen_top = top / 192.0F
        * static_cast<float>(viewport_height);
    const auto& glyph_pixels = chat::font::pixels();
    const auto& glyph_widths = chat::font::widths();

    glColor4f(static_cast<float>(red) / 255.0F,
              static_cast<float>(green) / 255.0F,
              static_cast<float>(blue) / 255.0F, alpha);
    glBegin(GL_QUADS);
    for (const char16_t value : text) {
        const std::int32_t glyph = chat::font::index(value);
        if (glyph < 0) {
            continue;
        }
        const std::size_t base = static_cast<std::size_t>(glyph)
            * static_cast<std::size_t>(chat::font::Cell * chat::font::Cell);
        if (value != u' ') {
            for (int row = 0; row < chat::font::Cell; ++row) {
                for (int column = 0; column < chat::font::Cell; ++column) {
                    const std::size_t pixel = base
                        + static_cast<std::size_t>(
                            row * chat::font::Cell + column);
                    if (glyph_pixels[pixel] == 0) {
                        continue;
                    }
                    const float x = cursor
                        + static_cast<float>(column) * pixel_scale;
                    const float y = screen_top
                        + static_cast<float>(row) * pixel_scale;
                    glVertex2f(x, y);
                    glVertex2f(x + pixel_scale, y);
                    glVertex2f(x + pixel_scale, y + pixel_scale);
                    glVertex2f(x, y + pixel_scale);
                }
            }
        }
        cursor += static_cast<float>(glyph_widths[
            static_cast<std::size_t>(glyph)]) * horizontal_scale;
    }
    glEnd();
#else
    static_cast<void>(text);
    static_cast<void>(left);
    static_cast<void>(top);
    static_cast<void>(aspect);
    static_cast<void>(viewport_width);
    static_cast<void>(viewport_height);
    static_cast<void>(alpha);
    static_cast<void>(red);
    static_cast<void>(green);
    static_cast<void>(blue);
#endif
}

} // namespace fruityprime::renderer::opengl
