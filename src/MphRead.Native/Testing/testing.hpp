#pragma once

#include "Metadata/metadata.hpp"

namespace MphReadNative::Testing {

// A small shared contract for source-tree smoke tests.  Feature-specific
// tests remain beside the native backend until their implementation modules
// move into the matching category.
constexpr bool has_metadata_contract() noexcept {
    return fruityprime::metadata::HunterCount == 8
        && fruityprime::metadata::WeaponCount == 9;
}

} // namespace MphReadNative::Testing
