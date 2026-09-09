#include "Entities/Players/hud_messages.hpp"

#include <limits>
#include <utility>

namespace fruityprime::players {

void HudMessageQueue::queue(float x, float y, Align align, int max_width,
                            float font_size, formats::ColorRgba color,
                            float alpha, float duration,
                            std::uint8_t category, std::string text,
                            int line_count, bool dialog_hide) {
    float min_duration = (std::numeric_limits<float>::max)();
    HudMessage* target = nullptr;
    for (HudMessage& existing : messages_) {
        if (existing.Lifetime > 0.0F) {
            if ((category & existing.Category & StackingCategories) != 0) {
                // Push the existing stack up by the height of what is being
                // posted, so the column stays readable.
                existing.Position.y -=
                    static_cast<float>(line_count) * existing.FontSize;
            } else if (existing.Position.y == y) {
                // Not stacking, same line: the old one goes.
                existing.Lifetime = 0.0F;
            }
        }
        // The slot that expires soonest is the one taken over.
        if (existing.Lifetime < min_duration) {
            min_duration = existing.Lifetime;
            target = &existing;
        }
    }
    if (target == nullptr) {
        return;
    }
    if ((category & StackingCategories) != 0) {
        // A stacking message grows upward from the anchor, so a multi-line
        // one starts higher rather than running off the bottom.
        y -= static_cast<float>(line_count - 1) * font_size;
    }
    target->Text = std::move(text);
    target->Position = {x, y};
    target->MaxWidth = max_width;
    target->FontSize = font_size;
    target->Color = color;
    target->Alpha = alpha;
    target->Alignment = align;
    target->Category = category;
    target->Lifetime = duration;
    target->DialogHide = dialog_hide;
}

void HudMessageQueue::process(float frame_time) noexcept {
    for (HudMessage& message : messages_) {
        if (message.Lifetime > 0.0F) {
            message.Lifetime -= frame_time;
            if (message.Lifetime < 0.0F) {
                message.Lifetime = 0.0F;
            }
        }
    }
}

void HudMessageQueue::clear(std::uint8_t category_mask) noexcept {
    for (HudMessage& message : messages_) {
        if ((message.Category & category_mask) != 0) {
            message.Lifetime = 0.0F;
        }
    }
}

} // namespace fruityprime::players
