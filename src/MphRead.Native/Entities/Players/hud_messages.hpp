#pragma once

// Native counterpart of PlayerHud's queued message system.
//
// The queue is a fixed pool of twenty slots, never a growing list: posting a
// message takes over whichever slot expires soonest, so a burst of messages
// overwrites the oldest rather than allocating.  Two rules make the stacking
// work, and both are easy to lose:
//
//   * a new message in one of the stacking categories pushes the existing
//     ones up by its own height, so they read as a column rather than
//     overlapping;
//   * a new message that is not stacking but sits at the same height kills
//     whatever was there, so two unrelated notices never share a line.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace fruityprime::players {

// PlayerHud.Align
enum class Align : std::uint8_t { Left = 0, Center = 1, Right = 2 };

// PlayerHud.HudMessage
struct HudMessage {
    formats::Vector2 Position{};
    float FontSize = 0.0F;
    formats::ColorRgba Color{};
    float Lifetime = 0.0F;
    float Alpha = 0.0F;
    std::uint8_t Category = 0;
    int MaxWidth = 0;
    Align Alignment = Align::Center;
    std::string Text;
    bool DialogHide = false;
};

class HudMessageQueue final {
public:
    static constexpr std::size_t Capacity = 20;
    // The category bits that stack rather than replace.
    static constexpr std::uint8_t StackingCategories = 14;

    [[nodiscard]] const std::array<HudMessage, Capacity>& messages()
        const noexcept {
        return messages_;
    }

    // PlayerHud.QueueHudMessage.  `line_count` is how many lines the text
    // wrapped to, which decides how far the stack is pushed.
    void queue(float x, float y, Align align, int max_width, float font_size,
               formats::ColorRgba color, float alpha, float duration,
               std::uint8_t category, std::string text, int line_count = 1,
               bool dialog_hide = false);

    // The managed overload that defaults everything but the essentials.
    void queue(float x, float y, float duration, std::uint8_t category,
               std::string text, int line_count = 1,
               bool dialog_hide = false) {
        queue(x, y, Align::Center, 256, 8.0F,
              formats::ColorRgba::from_rgb555(0x3FEF), 1.0F, duration,
              category, std::move(text), line_count, dialog_hide);
    }

    // PlayerHud.ProcessHudMessageQueue: every live message ages by one frame
    // and is clamped at zero rather than going negative, because the lifetime
    // is also what picks the slot to reuse.
    void process(float frame_time) noexcept;

    // PlayerHud.ClearHudMessage
    void clear(std::uint8_t category_mask) noexcept;

private:
    std::array<HudMessage, Capacity> messages_{};
};

} // namespace fruityprime::players
