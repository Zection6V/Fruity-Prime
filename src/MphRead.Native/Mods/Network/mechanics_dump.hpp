#pragma once

#include <iosfwd>

namespace fruityprime::mechanics {

// Print the live native metadata and the rules shared by the gameplay
// modules.  This is the native counterpart of Mods/Network/MechanicsDump.cs.
void print(std::ostream& output);

} // namespace fruityprime::mechanics
