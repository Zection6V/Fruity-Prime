#pragma once

namespace fruityprime::map_audit {

// Native counterpart of Mods/Network/MapAudit. Runs the headless room tour
// used by the command-line utility and keeps the map-test ownership out of
// Utility/main.cpp.
int run(int argc, char** argv);

} // namespace fruityprime::map_audit
