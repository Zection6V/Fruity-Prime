#include "Mods/Chat/chat_hud.hpp"

#include "Mods/Chat/chat_font.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace fruityprime::chat::hud {
namespace {

struct ChatRendererState {
    std::array<std::array<std::uint8_t, 4>, 2> palette{};
    bool palette_ready = false;
    bool character_ready = false;
    bool enabled = false;
};

ChatRendererState& renderer_state() noexcept {
    static ChatRendererState state;
    return state;
}

[[nodiscard]] bool decode_utf8(std::string_view text, std::size_t& offset,
                               std::uint32_t& code_point) noexcept {
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 0x80) {
        code_point = first;
        return true;
    }

    int continuation_count = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        continuation_count = 1;
        code_point = first & 0x1F;
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        continuation_count = 2;
        code_point = first & 0x0F;
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        continuation_count = 3;
        code_point = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (offset + static_cast<std::size_t>(continuation_count) > text.size()) {
        return false;
    }
    for (int count = 0; count < continuation_count; ++count) {
        const auto next = static_cast<unsigned char>(text[offset++]);
        if ((next & 0xC0) != 0x80) {
            return false;
        }
        code_point = (code_point << 6) | (next & 0x3F);
    }
    return code_point >= minimum && code_point <= 0x10FFFF
        && !(code_point >= 0xD800 && code_point <= 0xDFFF);
}

} // namespace

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

std::u16string utf8_to_utf16(const std::string_view text) {
    std::u16string result;
    result.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t sequence_start = offset;
        std::uint32_t code_point = 0;
        if (!decode_utf8(text, offset, code_point)) {
            // Invalid network text has no drawable glyph in the managed
            // font. Preserve one replacement code unit and continue rather
            // than letting a malformed byte sequence alter a cut boundary.
            offset = sequence_start + 1;
            result.push_back(u'\uFFFD');
            continue;
        }
        if (code_point <= 0xFFFF) {
            result.push_back(static_cast<char16_t>(code_point));
        } else {
            code_point -= 0x10000;
            result.push_back(static_cast<char16_t>(
                0xD800 + (code_point >> 10)));
            result.push_back(static_cast<char16_t>(
                0xDC00 + (code_point & 0x3FF)));
        }
    }
    return result;
}

float width(const std::u16string_view text, const float aspect) noexcept {
    return static_cast<float>(font::measure(text)) * Scale * aspect;
}

float room(const float aspect, const float used,
           const bool android) noexcept {
    return 256.0F - margin(android) * aspect * 2.0F - used;
}

std::u16string fit(const std::u16string_view text, const float aspect,
                   const float used, const bool android) {
    if (width(text, aspect) <= room(aspect, used, android)) {
        return std::u16string(text);
    }
    std::size_t count = text.size();
    while (count > 0
           && width(text.substr(0, count), aspect)
               > room(aspect, used, android)) {
        --count;
    }
    return std::u16string(text.substr(0, count));
}

std::u16string tail(const std::u16string_view text, const float aspect,
                    const float used, const bool android) {
    if (width(text, aspect) <= room(aspect, used, android)) {
        return std::u16string(text);
    }
    std::size_t start = 0;
    while (start < text.size()
           && width(text.substr(start), aspect)
               > room(aspect, used, android)) {
        ++start;
    }
    return std::u16string(text.substr(start));
}

void ensure_renderer() noexcept {
    auto& state = renderer_state();
    if (state.enabled) {
        return;
    }
    // PlayerEntityChatHud.cs: SetPaletteData first, then SetCharacterData,
    // then Enabled=true. The native backend draws the same pixels directly,
    // but preserves this initialization order and palette contract.
    state.palette[0] = {0, 0, 0, 0};
    state.palette[1] = {255, 255, 255, 255};
    state.palette_ready = true;
    static_cast<void>(font::pixels());
    state.character_ready = true;
    state.enabled = true;
}

} // namespace fruityprime::chat::hud
