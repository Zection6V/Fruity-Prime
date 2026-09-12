#include "RawFormats.hpp"

#include "Collision.hpp"
#include "Entity.hpp"

#include <cstring>
#include <memory>
#include <new>
#include <string>

namespace
{
    template <std::size_t N>
    [[nodiscard]] std::string MarshalString(
        const std::array<std::uint8_t, N>& array)
    {
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
    T& AssignReadonly(T& target, const T& source) noexcept
    {
        if (std::addressof(target) != std::addressof(source))
        {
            target.~T();
            ::new (static_cast<void*>(std::addressof(target))) T(source);
        }
        return target;
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

#define MPHREAD_DEFINE_READONLY_ASSIGNMENT(Type) \
    Type& Type::operator=(const Type& other) noexcept \
    { \
        return AssignReadonly(*this, other); \
    }

    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawMesh)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(DisplayList)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawMaterial)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(AnimationHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawMaterialAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawTextureAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawTexcoordAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawNodeAnimationGroup)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(MaterialAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TextureAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TexcoordAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(NodeAnimation)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Texture)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Palette)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Header)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawNode)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(CameraSequenceHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawCameraSequenceKeyframe)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawEffect)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawEffectElement)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(RawStringTableEntry)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TextFileEntry)

#undef MPHREAD_DEFINE_READONLY_ASSIGNMENT

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

    RawCollisionVolume::RawCollisionVolume() noexcept
        : Type{},
          BoxVector1{},
          BoxVector2{},
          BoxVector3{},
          BoxPosition{},
          BoxDot1{},
          BoxDot2{},
          BoxDot3{}
    {
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx boxVector1,
        Vector3Fx boxVector2,
        Vector3Fx boxVector3,
        Vector3Fx boxPosition,
        Fixed boxDot1,
        Fixed boxDot2,
        Fixed boxDot3) noexcept
        : RawCollisionVolume()
    {
        Type = VolumeType::Box;
        BoxVector1 = boxVector1;
        BoxVector2 = boxVector2;
        BoxVector3 = boxVector3;
        BoxPosition = boxPosition;
        BoxDot1 = boxDot1;
        BoxDot2 = boxDot2;
        BoxDot3 = boxDot3;
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx cylinderVector,
        Vector3Fx cylinderPosition,
        Fixed cylinderRadius,
        Fixed cylinderDot) noexcept
        : RawCollisionVolume()
    {
        Type = VolumeType::Cylinder;
        ::new (static_cast<void*>(std::addressof(CylinderVector)))
            Vector3Fx(cylinderVector);
        ::new (static_cast<void*>(std::addressof(CylinderPosition)))
            Vector3Fx(cylinderPosition);
        CylinderRadius.Value = cylinderRadius.Value;
        CylinderDot.Value = cylinderDot.Value;
    }

    RawCollisionVolume::RawCollisionVolume(
        Vector3Fx spherePosition,
        Fixed sphereRadius) noexcept
        : RawCollisionVolume()
    {
        Type = VolumeType::Sphere;
        ::new (static_cast<void*>(std::addressof(SpherePosition)))
            Vector3Fx(spherePosition);
        ::new (static_cast<void*>(std::addressof(SphereRadius)))
            Fixed(sphereRadius);
    }

    RawCollisionVolume::RawCollisionVolume(
        const RawCollisionVolume& other) noexcept
        : RawCollisionVolume()
    {
        std::memcpy(
            static_cast<void*>(this),
            static_cast<const void*>(std::addressof(other)),
            sizeof(*this));
    }

    RawCollisionVolume& RawCollisionVolume::operator=(
        const RawCollisionVolume& other) noexcept
    {
        if (this != std::addressof(other))
        {
            std::memcpy(
                static_cast<void*>(this),
                static_cast<const void*>(std::addressof(other)),
                sizeof(*this));
        }
        return *this;
    }

    FhRawCollisionVolume::FhRawCollisionVolume() noexcept
        : Type{},
          BoxPosition{},
          BoxVector1{},
          BoxVector2{},
          BoxVector3{},
          BoxDot1{},
          BoxDot2{},
          BoxDot3{}
    {
    }

    FhRawCollisionVolume::FhRawCollisionVolume(
        const FhRawCollisionVolume& other) noexcept
        : FhRawCollisionVolume()
    {
        std::memcpy(
            static_cast<void*>(this),
            static_cast<const void*>(std::addressof(other)),
            sizeof(*this));
    }

    FhRawCollisionVolume& FhRawCollisionVolume::operator=(
        const FhRawCollisionVolume& other) noexcept
    {
        if (this != std::addressof(other))
        {
            std::memcpy(
                static_cast<void*>(this),
                static_cast<const void*>(std::addressof(other)),
                sizeof(*this));
        }
        return *this;
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
