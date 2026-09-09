#include "Mods/Network/mechanics_dump.hpp"

#include <cassert>
#include <sstream>
#include <string>

int main() {
    std::ostringstream output;
    fruityprime::mechanics::print(output);
    const std::string text = output.str();
    assert(text.find("# Metroid Prime Hunters") != std::string::npos);
    assert(text.find("## Weapons (multiplayer table)") != std::string::npos);
    assert(text.find("| PowerBeam | 6 | 6 | 36 | 8 | 48 | 0/0 | UA | 0/0 | 5")
           != std::string::npos);
    assert(text.find("| Judicator | 24 | 12 | 12 | 32 | 12 | 12/0 | UA | 5/25 | 15 | charged: Freeze")
           != std::string::npos);
    assert(text.find("- Samus: Missile") != std::string::npos);
    assert(text.find("## Damage multipliers") != std::string::npos);
    assert(text.find("## Match modes") != std::string::npos);
    assert(text.find("## How multiplayer works here") != std::string::npos);
    return 0;
}
