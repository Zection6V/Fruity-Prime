#pragma once

// Native counterpart of Formats/Collision.cs.
// The MPH and First Hunt collision record layouts.
// Transliterated from the managed source so the field order and names stay
// checkable against it.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"


#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

using EntityLengthArray = std::array<std::uint16_t, 16>;

using Vector3FxArray10 = std::array<Vector3Fx, 10>;

using Vector3FxArray16 = std::array<Vector3Fx, 16>;

using Vector3FxArray8 = std::array<Vector3Fx, 8>;

using Vector4FxArray10 = std::array<Vector4Fx, 10>;

struct CollisionHeader {
    std::vector<char> Type;
    std::uint32_t PointCount{};
    std::uint32_t PointOffset{};
    std::uint32_t PlaneCount{};
    std::uint32_t PlaneOffset{};
    std::uint32_t PointIndexCount{};
    std::uint32_t PointIndexOffset{};
    std::uint32_t DataCount{};
    std::uint32_t DataOffset{};
    std::uint32_t DataIndexCount{};
    std::uint32_t DataIndexOffset{};
    std::int32_t PartsX{};
    std::int32_t PartsY{};
    std::int32_t PartsZ{};
    formats::Vector3Fx MinPosition{};
    std::uint32_t EntryCount{};
    std::uint32_t EntryOffset{};
    std::uint32_t PortalCount{};
    std::uint32_t PortalOffset{};
};

struct CollisionData {
    std::int32_t Counter{};
    std::uint16_t PlaneIndex{};
    CollisionFlags Flags{};
    std::uint16_t LayerMask{};
    std::uint16_t PaddingA{};
    std::uint16_t PointIndexCount{};
    std::uint16_t PointStartIndex{};
};

struct CollisionEntry {
    std::uint16_t DataCount{};
    std::uint16_t DataStartIndex{};
};

struct RawCollisionPortal {
    std::vector<char> Name;
    std::vector<char> NodeName1;
    std::vector<char> NodeName2;
    formats::Vector3Fx Point1{};
    formats::Vector3Fx Point2{};
    formats::Vector3Fx Point3{};
    formats::Vector3Fx Point4{};
    formats::Vector4Fx Plane1{};
    formats::Vector4Fx Plane2{};
    formats::Vector4Fx Plane3{};
    formats::Vector4Fx Plane4{};
    formats::Vector4Fx Plane{};
    std::uint16_t Flags{};
    std::uint16_t LayerMask{};
    std::uint16_t PointCount{};
    std::uint8_t UnusedDE{};
    std::uint8_t UnusedDF{};
};

struct FhCollisionPortal {
    std::vector<char> Name;
    std::vector<char> NodeName1;
    std::vector<char> NodeName2;
    formats::Vector4Fx Plane{};
    std::uint16_t VectorCount{};
    std::uint16_t VectorStartIndex{};
    std::uint8_t Field5C{};
    std::uint8_t Field5D{};
    std::uint16_t Padding5E{};
};

struct FhCollisionHeader {
    std::uint32_t PointCount{};
    std::uint32_t PointOffset{};
    std::uint32_t PlaneCount{};
    std::uint32_t PlaneOffset{};
    std::uint32_t VectorCount{};
    std::uint32_t VectorOffset{};
    std::uint16_t DataCount{};
    std::uint16_t DataStartIndex{};
    std::uint32_t DataOffset{};
    std::uint32_t DataIndexCount{};
    std::uint32_t DataIndexOffset{};
    std::uint32_t EntryCount{};
    std::uint32_t EntryOffset{};
    std::uint32_t TreeNodeIndexCount{};
    std::uint32_t TreeNodeIndexOffset{};
    std::uint32_t TreeNodeCount{};
    std::uint32_t TreeNodeOffset{};
    std::uint32_t PortalCount{};
    std::uint32_t PortalOffset{};
};

struct FhCollisionData {
    std::uint16_t PlaneIndex{};
    std::uint16_t VectorCount{};
    std::uint16_t VectorStartIndex{};
};

struct FhCollisionVector {
    std::uint16_t Point1Index{};
    std::uint16_t Point2Index{};
    std::uint16_t PlaneIndex{};
};

struct FhCollisionEntry {
    formats::Vector3Fx MinBounds{};
    formats::Vector3Fx MaxBounds{};
    std::uint16_t DataCount{};
    std::uint16_t DataStartIndex{};
};

struct FhCollisionTreeNode {
    formats::Vector3Fx MinBounds{};
    formats::Vector3Fx MaxBounds{};
    std::uint16_t LeftIndex{};
    std::uint16_t RightIndex{};
};

} // namespace fruityprime::formats
