#include "RawFormats.hpp"

#include "Collision.hpp"
#include "Entity.hpp"

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

namespace
{
    template <std::size_t N>
    [[nodiscard]] std::string MarshalString(
        const MphRead::NativeRuntime::ByValByteArray<N>& array)
    {
        // C# MarshalExtensions.MarshalString checks null before constructing
        // the managed string or inspecting any element.
        if (array.IsNull())
        {
            throw System::ArgumentNullException("array");
        }

        std::string result;
        result.reserve(N);
        for (std::uint8_t value : array)
        {
            if (value == 0)
            {
                break;
            }

            const std::uint32_t codePoint = value;
            if (codePoint <= 0x7FU)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else
            {
                result.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }
        return result;
    }

    template <typename T>
    T& AssignReadonly(T& target, const T& source)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        if (std::addressof(target) != std::addressof(source))
        {
            target.~T();
            ::new (static_cast<void*>(std::addressof(target))) T(source);
        }
        return target;
    }

    template <std::size_t Offset>
    [[nodiscard]] std::int32_t ReadInt32(
        const std::array<std::uint8_t, 64>& bytes) noexcept
    {
        static_assert(Offset + sizeof(std::int32_t) <= 64);
        std::int32_t value = 0;
        std::memcpy(
            static_cast<void*>(std::addressof(value)),
            bytes.data() + Offset,
            sizeof(value));
        return value;
    }

    template <std::size_t Offset>
    [[nodiscard]] std::uint32_t ReadUInt32(
        const std::array<std::uint8_t, 64>& bytes) noexcept
    {
        static_assert(Offset + sizeof(std::uint32_t) <= 64);
        std::uint32_t value = 0;
        std::memcpy(
            static_cast<void*>(std::addressof(value)),
            bytes.data() + Offset,
            sizeof(value));
        return value;
    }

    template <std::size_t Offset>
    [[nodiscard]] MphRead::Vector3Fx ReadVector3Fx(
        const std::array<std::uint8_t, 64>& bytes) noexcept
    {
        static_assert(Offset + 12 <= 64);
        return MphRead::Vector3Fx(
            ReadInt32<Offset>(bytes),
            ReadInt32<Offset + 4>(bytes),
            ReadInt32<Offset + 8>(bytes));
    }
}

namespace MphRead
{
    const std::int32_t Sizes::Header
        = static_cast<std::int32_t>(sizeof(MphRead::Header));
    const std::int32_t Sizes::Texture
        = static_cast<std::int32_t>(sizeof(MphRead::Texture));
    const std::int32_t Sizes::Palette
        = static_cast<std::int32_t>(sizeof(MphRead::Palette));
    const std::int32_t Sizes::Material
        = static_cast<std::int32_t>(sizeof(MphRead::RawMaterial));
    const std::int32_t Sizes::Node
        = static_cast<std::int32_t>(sizeof(MphRead::RawNode));
    const std::int32_t Sizes::Mesh
        = static_cast<std::int32_t>(sizeof(MphRead::RawMesh));
    const std::int32_t Sizes::Dlist
        = static_cast<std::int32_t>(sizeof(MphRead::DisplayList));
    const std::int32_t Sizes::EntityHeader
        = static_cast<std::int32_t>(sizeof(MphRead::EntityHeader));
    const std::int32_t Sizes::EntityEntry
        = static_cast<std::int32_t>(sizeof(MphRead::EntityEntry));
    const std::int32_t Sizes::FhEntityEntry
        = static_cast<std::int32_t>(sizeof(MphRead::FhEntityEntry));
    const std::int32_t Sizes::EntityDataHeader
        = static_cast<std::int32_t>(sizeof(MphRead::EntityDataHeader));
    const std::int32_t Sizes::JumpPadEntityData
        = static_cast<std::int32_t>(sizeof(MphRead::JumpPadEntityData));
    const std::int32_t Sizes::AnimationHeader
        = static_cast<std::int32_t>(sizeof(MphRead::AnimationHeader));
    const std::int32_t Sizes::NodeAnimation
        = static_cast<std::int32_t>(sizeof(MphRead::NodeAnimation));
    const std::int32_t Sizes::CameraSequenceHeader
        = static_cast<std::int32_t>(sizeof(MphRead::CameraSequenceHeader));
    const std::int32_t Sizes::CameraSequenceKeyframe
        = static_cast<std::int32_t>(sizeof(MphRead::RawCameraSequenceKeyframe));
    const std::int32_t Sizes::CollisionHeader
        = static_cast<std::int32_t>(
            sizeof(MphRead::Formats::Collision::CollisionHeader));
    const std::int32_t Sizes::FhCollisionHeader
        = static_cast<std::int32_t>(
            sizeof(MphRead::Formats::Collision::FhCollisionHeader));

#define MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(Type) \
    Type& Type::operator=(const Type& other) noexcept \
    { \
        return AssignReadonly(*this, other); \
    }

#define MPHREAD_DEFINE_READONLY_ASSIGNMENT(Type) \
    Type& Type::operator=(const Type& other) \
    { \
        return AssignReadonly(*this, other); \
    }

    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawMesh)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(DisplayList)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawMaterial)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(AnimationHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawMaterialAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawTextureAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawTexcoordAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawNodeAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(MaterialAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TextureAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TexcoordAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(NodeAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(Texture)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(Palette)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(Header)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawNode)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(CameraSequenceHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawCameraSequenceKeyframe)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(RawEffect)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawEffectElement)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawStringTableEntry)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT(TextFileEntry)

#undef MPHREAD_DEFINE_READONLY_ASSIGNMENT
#undef MPHREAD_DEFINE_READONLY_ASSIGNMENT_NOEXCEPT

    std::string RawMaterial::NameString() const
    {
        return MarshalString(Name);
    }

    std::string MaterialAnimation::NameString() const
    {
        return MarshalString(Name);
    }

    std::string TexcoordAnimation::NameString() const
    {
        return MarshalString(Name);
    }

    Texture::Texture(
        TextureFormat format,
        std::uint16_t width,
        std::uint16_t height) noexcept
        : Format(format),
          Padding1(0),
          Width(width),
          Height(height),
          Padding6(0),
          ImageOffset(0),
          ImageSize(0),
          UnusedOffset(0),
          UnusedCount(0),
          VramOffset(0),
          Opaque(1),
          SkipVram(0),
          PackedSize(0),
          NativeTextureFormat(0),
          ObjectRef(0)
    {
    }

    std::string RawNode::NameString() const
    {
        return MarshalString(Name);
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx boxVector1,
        Vector3Fx boxVector2,
        Vector3Fx boxVector3,
        Vector3Fx boxPosition,
        Fixed boxDot1,
        Fixed boxDot2,
        Fixed boxDot3) noexcept
        : NativeRuntime::RawCollisionTypeSlot(VolumeType::Box),
          NativeRuntime::RawCollisionOffset4(boxVector1),
          NativeRuntime::RawCollisionOffset16(boxVector2),
          NativeRuntime::RawCollisionOffset28(boxVector3),
          NativeRuntime::RawCollisionOffset40(
              boxPosition, boxDot1, boxDot2, boxDot3)
    {
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx cylinderVector,
        Vector3Fx cylinderPosition,
        Fixed cylinderRadius,
        Fixed cylinderDot) noexcept
        : NativeRuntime::RawCollisionTypeSlot(VolumeType::Cylinder),
          NativeRuntime::RawCollisionOffset4(cylinderVector),
          NativeRuntime::RawCollisionOffset16(cylinderPosition),
          NativeRuntime::RawCollisionOffset28(cylinderRadius, cylinderDot),
          NativeRuntime::RawCollisionOffset40()
    {
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx spherePosition,
        Fixed sphereRadius) noexcept
        : NativeRuntime::RawCollisionTypeSlot(VolumeType::Sphere),
          NativeRuntime::RawCollisionOffset4(spherePosition),
          NativeRuntime::RawCollisionOffset16(
              Vector3Fx(sphereRadius.Value, 0, 0)),
          NativeRuntime::RawCollisionOffset28(),
          NativeRuntime::RawCollisionOffset40()
    {
    }

    RawCollisionVolume::RawCollisionVolume(
        MarshaledTag,
        VolumeType type,
        Vector3Fx offset4,
        Vector3Fx offset16,
        Fixed offset28,
        Fixed offset32,
        std::int32_t offset36,
        Vector3Fx offset40,
        Fixed offset52,
        Fixed offset56,
        Fixed offset60) noexcept
        : NativeRuntime::RawCollisionTypeSlot(type),
          NativeRuntime::RawCollisionOffset4(offset4),
          NativeRuntime::RawCollisionOffset16(offset16),
          NativeRuntime::RawCollisionOffset28(offset28, offset32, offset36),
          NativeRuntime::RawCollisionOffset40(
              offset40, offset52, offset56, offset60)
    {
    }

    RawCollisionVolume RawCollisionVolume::FromMarshaledBytes(
        const std::array<std::uint8_t, 64>& bytes) noexcept
    {
        return RawCollisionVolume(
            MarshaledTag{},
            static_cast<VolumeType>(ReadUInt32<0>(bytes)),
            ReadVector3Fx<4>(bytes),
            ReadVector3Fx<16>(bytes),
            Fixed(ReadInt32<28>(bytes)),
            Fixed(ReadInt32<32>(bytes)),
            ReadInt32<36>(bytes),
            ReadVector3Fx<40>(bytes),
            Fixed(ReadInt32<52>(bytes)),
            Fixed(ReadInt32<56>(bytes)),
            Fixed(ReadInt32<60>(bytes)));
    }

    RawCollisionVolume& RawCollisionVolume::operator=(
        const RawCollisionVolume& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    FhRawCollisionVolume::FhRawCollisionVolume(
        MarshaledTag,
        FhVolumeType type,
        Vector3Fx offset4,
        Vector3Fx offset16,
        Fixed offset28,
        Fixed offset32,
        std::int32_t offset36,
        Vector3Fx offset40,
        Fixed offset52,
        Fixed offset56,
        Fixed offset60) noexcept
        : NativeRuntime::FhRawCollisionTypeSlot(type),
          NativeRuntime::FhRawCollisionOffset4(offset4),
          NativeRuntime::FhRawCollisionOffset16(offset16),
          NativeRuntime::FhRawCollisionOffset28(offset28, offset32, offset36),
          NativeRuntime::FhRawCollisionOffset40(
              offset40, offset52, offset56, offset60)
    {
    }

    FhRawCollisionVolume FhRawCollisionVolume::FromMarshaledBytes(
        const std::array<std::uint8_t, 64>& bytes) noexcept
    {
        return FhRawCollisionVolume(
            MarshaledTag{},
            static_cast<FhVolumeType>(ReadUInt32<0>(bytes)),
            ReadVector3Fx<4>(bytes),
            ReadVector3Fx<16>(bytes),
            Fixed(ReadInt32<28>(bytes)),
            Fixed(ReadInt32<32>(bytes)),
            ReadInt32<36>(bytes),
            ReadVector3Fx<40>(bytes),
            Fixed(ReadInt32<52>(bytes)),
            Fixed(ReadInt32<56>(bytes)),
            Fixed(ReadInt32<60>(bytes)));
    }

    FhRawCollisionVolume& FhRawCollisionVolume::operator=(
        const FhRawCollisionVolume& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    std::string RawCameraSequenceKeyframe::NodeNameString() const
    {
        return MarshalString(NodeName);
    }

    std::string RawEffectElement::NameString() const
    {
        return MarshalString(Name);
    }

    std::string RawEffectElement::ModelNameString() const
    {
        return MarshalString(ModelName);
    }

    std::string RawStringTableEntry::IdString() const
    {
        return MarshalString(Id);
    }
}
