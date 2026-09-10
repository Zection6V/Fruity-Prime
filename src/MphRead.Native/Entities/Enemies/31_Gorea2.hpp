#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::enemy::module_31_gorea_2 {

inline constexpr ModuleDescriptor kModule{
    31, "31_Gorea2.cs", "Enemy31Entity",
    PortStatus::PartialController, "update_gorea_2"};

// Enemy31Entity.Func21418EC: an up that stays square to a facing that
// has just turned.  The managed name is kept because nobody has worked
// out what to call it.
//
// It is the cross of the two, unless they are parallel -- in which case
// it tries the three world axes in turn rather than giving up, so a
// facing that has swung to point straight along one of them still gets a
// usable up out of the next.
[[nodiscard]] net::Vec3 Func21418EC(net::Vec3 vec1, net::Vec3 vec2) noexcept;

} // namespace fruityprime::enemy::module_31_gorea_2
