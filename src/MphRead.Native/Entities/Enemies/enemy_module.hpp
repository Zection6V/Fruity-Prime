#pragma once

#include <cstdint>
#include <string_view>

namespace fruityprime::enemy {

// A source-level status is intentionally separate from runtime support.  It
// makes the 1:1 managed/native inventory honest: a shared controller and a
// generic fallback are visible in the catalogue instead of looking like a
// completed class port.
enum class PortStatus : std::uint8_t {
    ImplementedController,
    SharedController,
    PartialController,
    GenericFallback,
    SpawnerBoundary
};

struct ModuleDescriptor {
    std::uint8_t id = 0;
    std::string_view managed_source;
    std::string_view managed_class;
    PortStatus status = PortStatus::GenericFallback;
    std::string_view native_controller;
};

} // namespace fruityprime::enemy
