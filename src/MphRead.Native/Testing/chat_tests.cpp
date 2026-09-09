#include "Mods/chat.hpp"
#include "Mods/Chat/chat_font.hpp"
#include "Mods/Chat/chat_hud.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

int main() {
    using fruityprime::chat::font::Cell;
    using fruityprime::chat::font::Count;
    using fruityprime::chat::font::First;
    using fruityprime::chat::font::index;
    using fruityprime::chat::font::measure;
    using fruityprime::chat::font::pixels;
    using fruityprime::chat::font::widths;
    assert(Count == 95);
    assert(Cell == 8);
    assert(index(First) == 0);
    assert(index('~') == 94);
    assert(index('\n') == -1);
    assert(widths()[static_cast<std::size_t>(index(' '))] == 3);
    assert(widths()[static_cast<std::size_t>(index('i'))] == 2);
    assert(widths()[static_cast<std::size_t>(index('A'))] == 6);
    assert(measure("Aim") == 14);
    const auto& glyphs = pixels();
    const std::size_t space = static_cast<std::size_t>(index(' ')) *
                              static_cast<std::size_t>(Cell * Cell);
    for (int pixel = 0; pixel < Cell * Cell; ++pixel) {
        assert(glyphs[space + static_cast<std::size_t>(pixel)] == 0);
    }
    const std::size_t a = static_cast<std::size_t>(index('A')) *
                          static_cast<std::size_t>(Cell * Cell);
    assert(glyphs[a + 1] == 1); // row 0, column 1 of the authored A.
    assert(glyphs[a + 0] == 0);

    assert(std::fabs(fruityprime::chat::hud::aspect_fix(1920, 1080)
                     - 0.75F) < 0.0001F);
    assert(fruityprime::chat::hud::margin(false) == 3.0F);
    assert(fruityprime::chat::hud::margin(true) == 30.0F);
    assert(fruityprime::chat::hud::clearance(false, 12.0F) == 12.0F);
    assert(fruityprime::chat::hud::clearance(true, 12.0F)
           == fruityprime::chat::hud::Bottom);
    assert(std::fabs(fruityprime::chat::hud::LineHeight - 4.35F)
           < 0.0001F);
    const std::string long_line(120, 'x');
    const std::string fitted = fruityprime::chat::hud::fit(long_line, 1.0F,
                                                            0.0F);
    const std::string tailed = fruityprime::chat::hud::tail(long_line, 1.0F,
                                                             0.0F);
    assert(fruityprime::chat::hud::width(fitted, 1.0F) <= 250.0F);
    assert(fruityprime::chat::hud::width(tailed, 1.0F) <= 250.0F);
    assert(fitted.size() < long_line.size());
    assert(tailed.size() < long_line.size());

    fruityprime::chat::Log log;
    log.add("Pilot", "hello", fruityprime::net::ChatPacket::KindSay, 0.0);
    log.add("Pilot", "still here", fruityprime::net::ChatPacket::KindSay,
            1.0);
    assert(log.visible(5.0).size() == 2);
    assert(log.visible(11.0).empty());

    log.open();
    log.handle_text('t'); // The key that opened the prompt is swallowed.
    log.handle_text('h');
    log.handle_text('i');
    assert(log.compose_text() == "hi");
    const std::string submitted = log.submit();
    assert(submitted == "hi");
    assert(!log.composing() && log.consume_just_closed());
    assert(!log.consume_just_closed());

    log.open(false);
    for (int i = 0; i < 100; ++i) {
        log.handle_text('x');
    }
    assert(log.compose_text().size() == fruityprime::chat::Log::MaxLength);
    assert(log.submit().size() == fruityprime::chat::Log::MaxLength);

    fruityprime::net::ChatPacket packet;
    packet.slot = 2;
    packet.name = "Sylux";
    packet.text = "incoming";
    log.receive(packet, 20.0);
    const auto lines = log.visible(20.0);
    assert(!lines.empty() && lines.back().line.name == "Sylux");
    std::cout << "native chat tests passed\n";
    return 0;
}
