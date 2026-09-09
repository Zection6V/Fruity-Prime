#pragma once

// Native counterpart of the shared value types in Formats/Formats.cs that the
// rest of the native tree did not already declare.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

struct FxFuncInfo {
    std::uint32_t FuncId{};
    std::vector<std::int32_t> Parameters;
};

struct StringTableEntry {
    std::string_view Id;
    char Prefix{};
    std::string_view Value1;
    std::string_view Value2;
    std::uint8_t Speed{};
    char Category{};
    std::string_view String1;
    std::string_view String2;
};

// Formats.CollisionVolume is an explicit-layout union: its box, cylinder and sphere members share storage.  The native record names every member instead, the way enemy_spawn::Fields already handles the same problem.
struct CollisionVolume {
    VolumeType Type{};
    formats::Vector3 BoxVector1{};
    formats::Vector3 BoxVector2{};
    formats::Vector3 BoxVector3{};
    formats::Vector3 BoxPosition{};
    float BoxDot1{};
    float BoxDot2{};
    float BoxDot3{};
    formats::Vector3 CylinderVector{};
    formats::Vector3 CylinderPosition{};
    float CylinderRadius{};
    float CylinderDot{};
    formats::Vector3 SpherePosition{};
    float SphereRadius{};

    // CollisionVolume.TestPoint: is this point inside the volume?  A box is
    // tested against its own three axes rather than an axis-aligned bound,
    // which is what lets an authored volume be rotated.
    [[nodiscard]] bool TestPoint(const formats::Vector3& point) const noexcept {
        if (Type == VolumeType::Box) {
            const formats::Vector3 difference = point - BoxPosition;
            const float dot1 = formats::dot(BoxVector1, difference);
            if (dot1 < 0.0F || dot1 > BoxDot1) {
                return false;
            }
            const float dot2 = formats::dot(BoxVector2, difference);
            if (dot2 < 0.0F || dot2 > BoxDot2) {
                return false;
            }
            const float dot3 = formats::dot(BoxVector3, difference);
            return dot3 >= 0.0F && dot3 <= BoxDot3;
        }
        if (Type == VolumeType::Cylinder) {
            const formats::Vector3 bottom = CylinderPosition;
            const formats::Vector3 top = bottom + CylinderVector * CylinderDot;
            const formats::Vector3 axis = top - bottom;
            if (formats::dot(point - bottom, axis) < 0.0F) {
                return false;
            }
            if (formats::dot(point - top, axis) > 0.0F) {
                return false;
            }
            const float length = axis.length();
            if (length == 0.0F) {
                return false;
            }
            return formats::cross(point - bottom, axis).length() / length
                <= CylinderRadius;
        }
        if (Type == VolumeType::Sphere) {
            return (SpherePosition - point).length() <= SphereRadius;
        }
        return false;
    }

    // CollisionVolume.GetCenter
    [[nodiscard]] formats::Vector3 GetCenter() const noexcept {
        if (Type == VolumeType::Box) {
            return BoxPosition + BoxVector1 * (BoxDot1 / 2.0F)
                + BoxVector2 * (BoxDot2 / 2.0F)
                + BoxVector3 * (BoxDot3 / 2.0F);
        }
        if (Type == VolumeType::Cylinder) {
            return CylinderPosition + CylinderVector * (CylinderDot / 2.0F);
        }
        return SpherePosition;
    }
};

// JumpPadDisplay is declared with the other display records in
// formats_display.hpp.

// EffectElement is declared with the effects port, which owns the Particle type it holds.

} // namespace fruityprime::formats
