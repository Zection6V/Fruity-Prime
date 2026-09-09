#include "Mods/Chat/chat_hud.hpp"

#include "Mods/Chat/chat_font.hpp"

#include <algorithm>

namespace fruityprime::chat::hud {

float aspect_fix(const int width, const int height) noexcept {
    if (width <= 0 || height <= 0) {
        return 1.0F;
    }
    return static_cast<float>(height) / 192.0F
        * (256.0F / static_cast<float>(width));
}

float margin(const bool android) noexcept {
    return android ? 30.0F : 3.0F;
}

float clearance(const bool available, const float position_y) noexcept {
    return available ? std::max(position_y, Bottom) : position_y;
}

float width(const std::string_view text, const float aspect) noexcept {
    return static_cast<float>(font::measure(text)) * Scale * aspect;
}

float room(const float aspect, const float used) noexcept {
    return 256.0F - margin(false) * aspect * 2.0F - used;
}

std::string fit(const std::string_view text, const float aspect,
                const float used) {
    if (width(text, aspect) <= room(aspect, used)) {
        return std::string(text);
    }
    std::size_t count = text.size();
    while (count > 0
           && width(text.substr(0, count), aspect) > room(aspect, used)) {
        --count;
    }
    return std::string(text.substr(0, count));
}

std::string tail(const std::string_view text, const float aspect,
                 const float used) {
    if (width(text, aspect) <= room(aspect, used)) {
        return std::string(text);
    }
    std::size_t start = 0;
    while (start < text.size()
           && width(text.substr(start), aspect) > room(aspect, used)) {
        ++start;
    }
    return std::string(text.substr(start));
}

} // namespace fruityprime::chat::hud
