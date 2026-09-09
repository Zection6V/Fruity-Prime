// PlayerHud's queued message system.
//
// Two stacking rules decide whether a burst of notices reads as a column or
// as one illegible pile, and both are easy to lose in a port: a stacking
// message pushes the existing ones up by its own height, and a non-stacking
// one on the same line replaces what was there.
#include "Entities/Players/hud_messages.hpp"
#include <cstdio>
int main() {
    using namespace fruityprime;
    players::HudMessageQueue q;
    // A stacking message (category bit within 14) pushes the existing one up.
    q.queue(128.0F, 150.0F, 2.0F, 2, "first");
    const float first_y = q.messages()[0].Position.y;
    q.queue(128.0F, 150.0F, 2.0F, 2, "second");
    float pushed = 0.0F, second = 0.0F;
    for (const auto& m : q.messages()) {
        if (m.Text == "first") { pushed = m.Position.y; }
        if (m.Text == "second") { second = m.Position.y; }
    }
    // A non-stacking message on the same line kills what was there.
    q.queue(128.0F, 150.0F, 2.0F, 1, "replace");
    int alive_at_line = 0;
    for (const auto& m : q.messages()) {
        if (m.Lifetime > 0.0F && m.Position.y == 150.0F) { ++alive_at_line; }
    }
    // Ageing clamps at zero rather than going negative.
    q.process(10.0F);
    float lowest = 1.0F;
    for (const auto& m : q.messages()) { if (m.Lifetime < lowest) lowest = m.Lifetime; }
    std::printf("native hud queue: first_y=%.1f pushed=%.1f second=%.1f "
                "alive_at_line=%d lowest=%.1f slots=%zu\n",
        first_y, pushed, second, alive_at_line, lowest, q.messages().size());
    return (first_y == 150.0F && pushed < first_y && second == 150.0F
            && alive_at_line == 1 && lowest == 0.0F
            && q.messages().size() == 20) ? 0 : 1;
}
