#include "Mods/Input/input.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool has(fruityprime::net::IntentButtons buttons,
         fruityprime::net::IntentButtons bit) {
    return (static_cast<std::uint32_t>(buttons)
            & static_cast<std::uint32_t>(bit)) != 0;
}

} // namespace

int main() {
    try {
        fruityprime::input::State state;
        state.set(fruityprime::input::Action::MoveUp, true);
        state.set(fruityprime::input::Action::Shoot, true);
        state.set(fruityprime::input::Action::Scan, true);
        require(has(state.buttons(), fruityprime::net::IntentButtons::MoveUp),
                "move action did not map to its network bit");
        require(has(state.buttons(), fruityprime::net::IntentButtons::Shoot),
                "shoot action did not map to its network bit");
        require(state.scan(), "local scan action was not retained");
        state.set(fruityprime::input::Action::Scan, false);
        require(!state.scan(), "released local scan action remained set");
        state.set(fruityprime::input::Action::MoveUp, false);
        require(!has(state.buttons(), fruityprime::net::IntentButtons::MoveUp),
                "released action remained set");
        require(has(state.buttons(), fruityprime::net::IntentButtons::Shoot),
                "changing one action cleared another action");

        state.set_aim({3.0F, 0.0F, 4.0F});
        const auto aim = state.aim();
        require(std::abs(aim.x - 0.6F) < 0.0001F
                    && std::abs(aim.z - 0.8F) < 0.0001F,
                "aim was not normalized");
        state.set_aim({0.0F, 0.0F, 0.0F});
        require(state.aim().z == 1.0F,
                "zero aim did not use the forward fallback");
        state.clear();
        require(state.buttons() == fruityprime::net::IntentButtons::None,
                "clear did not reset buttons");
        require(state.aim().z == 1.0F, "clear did not reset aim");
        require(!state.scan(), "clear did not reset local scan state");
        std::cout << "input state tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
