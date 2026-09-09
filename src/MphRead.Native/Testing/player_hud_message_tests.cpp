// PlayerHud's queued messages: the word wrapping and the twenty-slot queue.
//
// Wrapping is the part with real logic.  A line breaks at the last space
// that fits, and the width already written past that space carries over to
// the new line -- get that carry wrong and long text drifts one word further
// right on every line rather than wrapping squarely.  A line with no space
// to break at breaks after the character that overflowed instead.
//
// The font is synthetic here on purpose: eight units per character means a
// width in this test reads as a character count, so what is being pinned is
// where the breaks land and not what the cartridge's glyphs measure.
#include "Entities/Players/PlayerHud.hpp"
#include "Strings.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

void install_fixed_width_font() {
    // Printable ASCII, eight units wide, no vertical offset.
    constexpr std::size_t count = 96;
    std::vector<std::uint8_t> widths(count, 8);
    std::vector<std::uint8_t> offsets(count, 0);
    fruityprime::strings::Font::normal().set_data(
        widths, offsets, {}, 0x20, false);
}

int count_lines(const std::string& text) {
    int lines = 1;
    for (const char letter : text) {
        if (letter == '\n') {
            ++lines;
        }
    }
    return lines;
}

} // namespace

int main() {
    using fruityprime::players::HudMessageQueue;
    install_fixed_width_font();

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native HUD message tests: %s\n", what);
            ++failures;
        }
    };

    // Five characters fit in forty units, so the break lands at the space
    // before "world" and the new line starts empty.
    std::string wrapped;
    int lines = HudMessageQueue::WrapText("hello world", 40, wrapped);
    check(lines == 2, "hello world did not wrap into two lines");
    check(wrapped == "hello\nworld", "hello world broke in the wrong place");

    // A width nothing can fit in leaves the text alone rather than looping.
    lines = HudMessageQueue::WrapText("hello world", 0, wrapped);
    check(lines == 1, "a zero width should wrap nothing");

    // One word longer than the line has no space to break at, so it breaks
    // after the character that overflowed.
    lines = HudMessageQueue::WrapText("abcdefgh", 32, wrapped);
    check(lines == 2, "an unbreakable word did not break at all");
    check(count_lines(wrapped) == lines,
          "the reported line count disagrees with the text");

    // Newlines already in the text count, and reset the measurement.
    lines = HudMessageQueue::WrapText("ab\ncd", 400, wrapped);
    check(lines == 2, "an existing newline was not counted");
    check(wrapped == "ab\ncd", "an existing newline was rewritten");

    // Three words that fit two to a line.
    lines = HudMessageQueue::WrapText("one two six", 64, wrapped);
    check(lines == 2, "the carry past a break is wrong");
    check(wrapped == "one two\nsix", "the second break landed wrong");

    // The queue.  Two messages in the same stack push the earlier one up by
    // its own font size per line; a message at the same position in another
    // stack is retired instead.
    HudMessageQueue queue;
    queue.QueueHudMessage(128.0F, 150.0F, 2.0F, 2, "first");
    const float first_y = queue.messages()[0].y;
    queue.QueueHudMessage(128.0F, 150.0F, 2.0F, 2, "second");
    check(queue.messages()[0].y < first_y,
          "a second message in the same stack did not push the first up");
    check(queue.IsHudMessageQueued(2), "the queue lost its own messages");
    check(!queue.IsHudMessageQueued(16), "a mask matched the wrong category");

    // Lifetime runs out on the frame clock and stops at zero.
    queue.ProcessHudMessageQueue(1.0F);
    check(queue.IsHudMessageQueued(2), "a two second message died in one");
    queue.ProcessHudMessageQueue(5.0F);
    check(!queue.IsHudMessageQueued(2), "a message outlived its duration");
    for (const auto& message : queue.messages()) {
        check(message.lifetime >= 0.0F, "a lifetime went negative");
    }

    queue.QueueHudMessage(128.0F, 133.0F, 2.0F, 16, "acquiring node");
    check(queue.IsHudMessageQueued(16), "the node message was not queued");
    queue.ClearHudMessage(16);
    check(!queue.IsHudMessageQueued(16), "clearing by mask did nothing");

    // Twenty-one messages fit in twenty slots: the shortest-lived is reused
    // rather than the queue refusing the last one.
    HudMessageQueue full;
    for (int index = 0; index < 21; ++index) {
        full.QueueHudMessage(0.0F, static_cast<float>(index), 5.0F, 32,
                             "x");
    }
    int alive = 0;
    for (const auto& message : full.messages()) {
        if (message.lifetime > 0.0F) {
            ++alive;
        }
    }
    check(alive == static_cast<int>(HudMessageQueue::Capacity),
          "the queue did not stay full");

    if (failures == 0) {
        std::printf("native HUD message tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
