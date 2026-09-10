#pragma once

#include "Entities/gameplay.hpp"

namespace fruityprime::net {

class NetPlayerSetup final {
public:
    static void Reset() noexcept;
    static void ApplyOnce(gameplay::Session& session, int local_slot) noexcept;

private:
    inline static bool applied_ = false;
};

} // namespace fruityprime::net

namespace MphReadNative::Mods::Network {
using NetPlayerSetup = ::fruityprime::net::NetPlayerSetup;
}
