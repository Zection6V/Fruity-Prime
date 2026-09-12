#pragma once

#include "Effects.hpp"
#include "Enums.hpp"
#include "Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace MphRead
{
    class Sizes final
    {
    public:
        static const std::int32_t Header;
        static const std::int32_t Texture;
        static const std::int32_t Palette;
        static const std::int32_t Material;
        static const std::int32_t Node;
        static const std::int32_t Mesh;
        static const std::int32_t Dlist;
        static const std::int32_t EntityHeader;
        static const std::int32_t EntityEntry;
        static const std::int32_t FhEntityEntry;
        static const std::int32_t EntityDataHeader;
        static const std::int32_t JumpPadEntityData;
        static const std::int32_t AnimationHeader;
        static const std::int32_t NodeAnimation;
        static const std::int32_t CameraSequenceHeader;
        static const std::int32_t CameraSequenceKeyframe;
        static const std::int32_t CollisionHeader;
        static const std::int32_t FhCollisionHeader;

        Sizes() = delete;
    };

    struct RawMesh
    {
        const std::uint16_t MaterialId = 0;
        const std::uint16_t DlistId = 0;

        RawMesh() noexcept = default;
        RawMesh(const RawMesh&) noexcept = default;
        RawMesh& operator=(const RawMesh& other) noexcept;
    };

    struct DisplayList
    {
        const std::uint32_t Offset = 0;
        const std::uint32_t Size = 0;
        const Vector3Fx MinBounds{};
        const Vector3Fx MaxBounds{};

        DisplayList() noexcept = default;
        DisplayList(const DisplayList&) noexcept = default;
        DisplayList& operator=(const DisplayList& other) noexcept;
    };

    struct RawMaterial
    {
        const std::array<std::uint8_t, 64> Name{};
        const std::uint8_t Lighting = 0;
        const CullingMode Culling{};
        const std::uint8_t Alpha = 0;
        const std::uint8_t Wireframe = 0;
        const std::int16_t PaletteId = 0;
        const std::int16_t TextureId = 0;
        const RepeatMode XRepeat{};
        const RepeatMode YRepeat{};
        const ColorRgb Diffuse{};
        const ColorRgb Ambient{};
        const ColorRgb Specular{};
        const std::uint8_t Padding53 = 0;
        const MphRead::PolygonMode PolygonMode{};
        const MphRead::RenderMode RenderMode{};
        const std::uint8_t AnimationFlags = 0;
        const std::uint16_t Padding5A = 0;
        const TexgenMode TexcoordTransformMode{};
        const std::uint16_t TexcoordAnimationId = 0;
        const std::uint16_t Padding62 = 0;
        const std::uint32_t MatrixId = 0;
        const Fixed ScaleS{};
        const Fixed ScaleT{};
        const std::uint16_t RotateZ = 0;
        const std::uint16_t Padding72 = 0;
        const Fixed TranslateS{};
        const Fixed TranslateT{};
        const std::uint16_t MaterialAnimationId = 0;
        const std::uint16_t TextureAnimationId = 0;
        const std::uint8_t PackedRepeatMode = 0;
        const std::uint8_t Padding81 = 0;
        const std::uint16_t Padding82 = 0;

        RawMaterial() noexcept = default;
        RawMaterial(const RawMaterial&) noexcept = default;
        RawMaterial& operator=(const RawMaterial& other) noexcept;

        [[nodiscard]] std::string NameString() const;
    };

    struct AnimationHeader
    {
        const std::uint32_t NodeGroupOffset = 0;
        const std::uint32_t UnusedGroupOffset = 0;
        const std::uint32_t MaterialGroupOffset = 0;
        const std::uint32_t TexcoordGroupOffset = 0;
        const std::uint32_t TextureGroupOffset = 0;
        const std::uint16_t Count = 0;
        const std::uint16_t Padding16 = 0;

        AnimationHeader() noexcept = default;
        AnimationHeader(const AnimationHeader&) noexcept = default;
        AnimationHeader& operator=(const AnimationHeader& other) noexcept;
    };

    struct RawMaterialAnimationGroup
    {
        const std::uint32_t FrameCount = 0;
        const std::uint32_t ColorLutOffset = 0;
        const std::uint32_t AnimationCount = 0;
        const std::uint32_t AnimationOffset = 0;
        const std::uint16_t AnimationFrame = 0;
        const std::uint16_t Unused12 = 0;

        RawMaterialAnimationGroup() noexcept = default;
        RawMaterialAnimationGroup(const RawMaterialAnimationGroup&) noexcept = default;
        RawMaterialAnimationGroup& operator=(const RawMaterialAnimationGroup& other) noexcept;
    };

    struct RawTextureAnimationGroup
    {
        const std::uint16_t FrameCount = 0;
        const std::uint16_t FrameIndexCount = 0;
        const std::uint16_t TextureIdCount = 0;
        const std::uint16_t PaletteIdCount = 0;
        const std::uint16_t AnimationCount = 0;
        const std::uint16_t UnusedA = 0;
        const std::uint32_t FrameIndexOffset = 0;
        const std::uint32_t TextureIdOffset = 0;
        const std::uint32_t PaletteIdOffset = 0;
        const std::uint32_t AnimationOffset = 0;
        const std::uint16_t AnimationFrame = 0;
        const std::uint16_t Unused1C = 0;

        RawTextureAnimationGroup() noexcept = default;
        RawTextureAnimationGroup(const RawTextureAnimationGroup&) noexcept = default;
        RawTextureAnimationGroup& operator=(const RawTextureAnimationGroup& other) noexcept;
    };

    struct RawTexcoordAnimationGroup
    {
        const std::uint32_t FrameCount = 0;
        const std::uint32_t ScaleLutOffset = 0;
        const std::uint32_t RotateLutOffset = 0;
        const std::uint32_t TranslateLutOffset = 0;
        const std::uint32_t AnimationCount = 0;
        const std::uint32_t AnimationOffset = 0;
        const std::uint16_t AnimationFrame = 0;
        const std::uint16_t Unused1A = 0;

        RawTexcoordAnimationGroup() noexcept = default;
        RawTexcoordAnimationGroup(const RawTexcoordAnimationGroup&) noexcept = default;
        RawTexcoordAnimationGroup& operator=(const RawTexcoordAnimationGroup& other) noexcept;
    };

    struct RawNodeAnimationGroup
    {
        const std::uint32_t FrameCount = 0;
        const std::uint32_t ScaleLutOffset = 0;
        const std::uint32_t RotateLutOffset = 0;
        const std::uint32_t TranslateLutOffset = 0;
        const std::uint32_t AnimationOffset = 0;

        RawNodeAnimationGroup() noexcept = default;
        RawNodeAnimationGroup(const RawNodeAnimationGroup&) noexcept = default;
        RawNodeAnimationGroup& operator=(const RawNodeAnimationGroup& other) noexcept;
    };

    struct MaterialAnimation
    {
        const std::array<std::uint8_t, 64> Name{};
        const std::uint32_t Unused40 = 0;
        const std::uint8_t DiffuseBlendR = 0;
        const std::uint8_t DiffuseBlendG = 0;
        const std::uint8_t DiffuseBlendB = 0;
        const std::uint8_t Unused47 = 0;
        const std::uint16_t DiffuseLutLengthR = 0;
        const std::uint16_t DiffuseLutLengthG = 0;
        const std::uint16_t DiffuseLutLengthB = 0;
        const std::uint16_t DiffuseLutIndexR = 0;
        const std::uint16_t DiffuseLutIndexG = 0;
        const std::uint16_t DiffuseLutIndexB = 0;
        const std::uint8_t AmbientBlendR = 0;
        const std::uint8_t AmbientBlendG = 0;
        const std::uint8_t AmbientBlendB = 0;
        const std::uint8_t Unused57 = 0;
        const std::uint16_t AmbientLutLengthR = 0;
        const std::uint16_t AmbientLutLengthG = 0;
        const std::uint16_t AmbientLutLengthB = 0;
        const std::uint16_t AmbientLutIndexR = 0;
        const std::uint16_t AmbientLutIndexG = 0;
        const std::uint16_t AmbientLutIndexB = 0;
        const std::uint8_t SpecularBlendR = 0;
        const std::uint8_t SpecularBlendG = 0;
        const std::uint8_t SpecularBlendB = 0;
        const std::uint8_t Unused67 = 0;
        const std::uint16_t SpecularLutLengthR = 0;
        const std::uint16_t SpecularLutLengthG = 0;
        const std::uint16_t SpecularLutLengthB = 0;
        const std::uint16_t SpecularLutIndexR = 0;
        const std::uint16_t SpecularLutIndexG = 0;
        const std::uint16_t SpecularLutIndexB = 0;
        const std::uint32_t Unused74 = 0;
        const std::uint32_t Unused78 = 0;
        const std::uint32_t Unused7C = 0;
        const std::uint32_t Unused80 = 0;
        const std::uint8_t AlphaBlend = 0;
        const std::uint8_t Unused85 = 0;
        const std::uint16_t AlphaLutLength = 0;
        const std::uint16_t AlphaLutIndex = 0;
        const std::uint16_t MaterialId = 0;

        MaterialAnimation() noexcept = default;
        MaterialAnimation(const MaterialAnimation&) noexcept = default;
        MaterialAnimation& operator=(const MaterialAnimation& other) noexcept;

        [[nodiscard]] std::string NameString() const;
    };

    struct TextureAnimation
    {
        const std::array<std::uint8_t, 32> Name{};
        const std::uint16_t Count = 0;
        const std::uint16_t StartIndex = 0;
        const std::uint16_t MinimumPaletteId = 0;
        const std::uint16_t MaterialId = 0;
        const std::uint16_t MinimumTextureId = 0;
        const std::uint16_t Field2A = 0;

        TextureAnimation() noexcept = default;
        TextureAnimation(const TextureAnimation&) noexcept = default;
        TextureAnimation& operator=(const TextureAnimation& other) noexcept;
    };

    struct TexcoordAnimation
    {
        const std::array<std::uint8_t, 32> Name{};
        const std::uint8_t ScaleBlendS = 0;
        const std::uint8_t ScaleBlendT = 0;
        const std::uint16_t ScaleLutLengthS = 0;
        const std::uint16_t ScaleLutLengthT = 0;
        const std::uint16_t ScaleLutIndexS = 0;
        const std::uint16_t ScaleLutIndexT = 0;
        const std::uint8_t RotateBlendZ = 0;
        const std::uint8_t Unused2B = 0;
        const std::uint16_t RotateLutLengthZ = 0;
        const std::uint16_t RotateLutIndexZ = 0;
        const std::uint8_t TranslateBlendS = 0;
        const std::uint8_t TranslateBlendT = 0;
        const std::uint16_t TranslateLutLengthS = 0;
        const std::uint16_t TranslateLutLengthT = 0;
        const std::uint16_t TranslateLutIndexS = 0;
        const std::uint16_t TranslateLutIndexT = 0;
        const std::uint16_t Padding3A = 0;

        TexcoordAnimation() noexcept = default;
        TexcoordAnimation(const TexcoordAnimation&) noexcept = default;
        TexcoordAnimation& operator=(const TexcoordAnimation& other) noexcept;

        [[nodiscard]] std::string NameString() const;
    };

    struct NodeAnimation
    {
        const std::uint8_t ScaleBlendX = 0;
        const std::uint8_t ScaleBlendY = 0;
        const std::uint8_t ScaleBlendZ = 0;
        const std::uint8_t Flags = 0;
        const std::uint16_t ScaleLutLengthX = 0;
        const std::uint16_t ScaleLutLengthY = 0;
        const std::uint16_t ScaleLutLengthZ = 0;
        const std::uint16_t ScaleLutIndexX = 0;
        const std::uint16_t ScaleLutIndexY = 0;
        const std::uint16_t ScaleLutIndexZ = 0;
        const std::uint8_t RotateBlendX = 0;
        const std::uint8_t RotateBlendY = 0;
        const std::uint8_t RotateBlendZ = 0;
        const std::uint8_t Padding13 = 0;
        const std::uint16_t RotateLutLengthX = 0;
        const std::uint16_t RotateLutLengthY = 0;
        const std::uint16_t RotateLutLengthZ = 0;
        const std::uint16_t RotateLutIndexX = 0;
        const std::uint16_t RotateLutIndexY = 0;
        const std::uint16_t RotateLutIndexZ = 0;
        const std::uint8_t TranslateBlendX = 0;
        const std::uint8_t TranslateBlendY = 0;
        const std::uint8_t TranslateBlendZ = 0;
        const std::uint8_t Padding23 = 0;
        const std::uint16_t TranslateLutLengthX = 0;
        const std::uint16_t TranslateLutLengthY = 0;
        const std::uint16_t TranslateLutLengthZ = 0;
        const std::uint16_t TranslateLutIndexX = 0;
        const std::uint16_t TranslateLutIndexY = 0;
        const std::uint16_t TranslateLutIndexZ = 0;

        NodeAnimation() noexcept = default;
        NodeAnimation(const NodeAnimation&) noexcept = default;
        NodeAnimation& operator=(const NodeAnimation& other) noexcept;
    };

    struct Texture
    {
        const TextureFormat Format{};
        const std::uint8_t Padding1 = 0;
        const std::uint16_t Width = 0;
        const std::uint16_t Height = 0;
        const std::uint16_t Padding6 = 0;
        const std::uint32_t ImageOffset = 0;
        const std::uint32_t ImageSize = 0;
        const std::uint32_t UnusedOffset = 0;
        const std::uint32_t UnusedCount = 0;
        const std::uint32_t VramOffset = 0;
        const std::uint32_t Opaque = 0;
        const std::uint32_t SkipVram = 0;
        const std::uint8_t PackedSize = 0;
        const std::uint8_t NativeTextureFormat = 0;
        const std::uint16_t ObjectRef = 0;

        Texture() noexcept = default;
        Texture(TextureFormat format, std::uint16_t width, std::uint16_t height) noexcept;
        Texture(const Texture&) noexcept = default;
        Texture& operator=(const Texture& other) noexcept;
    };

    struct Palette
    {
        const std::uint32_t Offset = 0;
        const std::uint32_t Size = 0;
        const std::uint32_t VramOffset = 0;
        const std::uint32_t ObjectRef = 0;

        Palette() noexcept = default;
        Palette(const Palette&) noexcept = default;
        Palette& operator=(const Palette& other) noexcept;
    };

    struct Header
    {
        const std::uint32_t ScaleFactor = 0;
        const Fixed ScaleBase{};
        const std::uint32_t PrimitiveCount = 0;
        const std::uint32_t VertexCount = 0;
        const std::uint32_t MaterialOffset = 0;
        const std::uint32_t DlistOffset = 0;
        const std::uint32_t NodeOffset = 0;
        const std::uint16_t NodeWeightCount = 0;
        const std::uint8_t Flags = 0;
        const std::uint8_t Padding1F = 0;
        const std::uint32_t NodeWeightOffset = 0;
        const std::uint32_t MeshOffset = 0;
        const std::uint16_t TextureCount = 0;
        const std::uint16_t Padding2A = 0;
        const std::uint32_t TextureOffset = 0;
        const std::uint16_t PaletteCount = 0;
        const std::uint16_t Padding32 = 0;
        const std::uint32_t PaletteOffset = 0;
        const std::uint32_t NodePosCounts = 0;
        const std::uint32_t NodePosScales = 0;
        const std::uint32_t NodeInitialPosition = 0;
        const std::uint32_t NodePosition = 0;
        const std::uint16_t MaterialCount = 0;
        const std::uint16_t NodeCount = 0;
        const std::uint32_t TextureMatrixOffset = 0;
        const std::uint32_t NodeAnimationOffset = 0;
        const std::uint32_t TextureCoordinateAnimations = 0;
        const std::uint32_t MaterialAnimations = 0;
        const std::uint32_t TextureAnimations = 0;
        const std::uint16_t MeshCount = 0;
        const std::uint16_t TextureMatrixCount = 0;

        Header() noexcept = default;
        Header(const Header&) noexcept = default;
        Header& operator=(const Header& other) noexcept;
    };

    struct RawNode
    {
        const std::array<std::uint8_t, 64> Name{};
        const std::int16_t ParentId = 0;
        const std::int16_t ChildId = 0;
        const std::int16_t NextId = 0;
        const std::uint16_t Padding46 = 0;
        const std::uint32_t Enabled = 0;
        const std::uint16_t MeshCount = 0;
        const std::uint16_t MeshId = 0;
        const Vector3Fx Scale{};
        const std::int16_t AngleX = 0;
        const std::int16_t AngleY = 0;
        const std::int16_t AngleZ = 0;
        const std::uint16_t Padding62 = 0;
        const Vector3Fx Position{};
        const Fixed BoundingRadius{};
        const Vector3Fx MinBounds{};
        const Vector3Fx MaxBounds{};
        const MphRead::BillboardMode BillboardMode{};
        const std::uint8_t Padding8D = 0;
        const std::uint16_t Padding8E = 0;
        const Matrix43Fx Transform{};
        const std::uint32_t BeforeTransform = 0;
        const std::uint32_t AfterTransform = 0;
        const std::uint32_t UnusedC8 = 0;
        const std::uint32_t UnusedCC = 0;
        const std::uint32_t UnusedD0 = 0;
        const std::uint32_t UnusedD4 = 0;
        const std::uint32_t UnusedD8 = 0;
        const std::uint32_t UnusedDC = 0;
        const std::uint32_t UnusedE0 = 0;
        const std::uint32_t UnusedE4 = 0;
        const std::uint32_t UnusedE8 = 0;
        const std::uint32_t UnusedEC = 0;

        RawNode() noexcept = default;
        RawNode(const RawNode&) noexcept = default;
        RawNode& operator=(const RawNode& other) noexcept;

        [[nodiscard]] std::string NameString() const;
    };

    struct RawCollisionVolume
    {
    private:
        struct ExplicitFixedField
        {
            std::int32_t Value;

            [[nodiscard]] float FloatValue() const noexcept
            {
                return Fixed::ToFloat(Value);
            }

            [[nodiscard]] operator Fixed() const noexcept
            {
                return Fixed(Value);
            }

            [[nodiscard]] std::string ToString() const
            {
                return Fixed(Value).ToString();
            }
        };

    public:
        VolumeType Type{};

        union
        {
            Vector3Fx BoxVector1;
            Vector3Fx CylinderVector;
            Vector3Fx SpherePosition;
        };

        union
        {
            Vector3Fx BoxVector2;
            Vector3Fx CylinderPosition;
            Fixed SphereRadius;
        };

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
        union
        {
            Vector3Fx BoxVector3;
            struct
            {
                ExplicitFixedField CylinderRadius;
                ExplicitFixedField CylinderDot;
            };
        };
#else
#error "RawCollisionVolume requires compiler support for anonymous struct members to preserve C# explicit-layout field names."
#endif

        Vector3Fx BoxPosition{};
        Fixed BoxDot1{};
        Fixed BoxDot2{};
        Fixed BoxDot3{};

        RawCollisionVolume() noexcept;
        RawCollisionVolume(
            Vector3Fx boxVector1,
            Vector3Fx boxVector2,
            Vector3Fx boxVector3,
            Vector3Fx boxPosition,
            Fixed boxDot1,
            Fixed boxDot2,
            Fixed boxDot3) noexcept;
        RawCollisionVolume(
            Vector3Fx cylinderVector,
            Vector3Fx cylinderPosition,
            Fixed cylinderRadius,
            Fixed cylinderDot) noexcept;
        RawCollisionVolume(Vector3Fx spherePosition, Fixed sphereRadius) noexcept;
        RawCollisionVolume(const RawCollisionVolume& other) noexcept;
        RawCollisionVolume& operator=(const RawCollisionVolume& other) noexcept;
    };

    struct FhRawCollisionVolume
    {
    private:
        struct ExplicitFixedField
        {
            std::int32_t Value;

            [[nodiscard]] float FloatValue() const noexcept
            {
                return Fixed::ToFloat(Value);
            }

            [[nodiscard]] operator Fixed() const noexcept
            {
                return Fixed(Value);
            }

            [[nodiscard]] std::string ToString() const
            {
                return Fixed(Value).ToString();
            }
        };

    public:
        FhVolumeType Type{};

        union
        {
            Vector3Fx BoxPosition;
            Vector3Fx CylinderPosition;
            Vector3Fx SpherePosition;
        };

        union
        {
            Vector3Fx BoxVector1;
            Vector3Fx CylinderVector;
            Fixed SphereRadius;
        };

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
        union
        {
            Vector3Fx BoxVector2;
            struct
            {
                ExplicitFixedField CylinderDot;
                ExplicitFixedField CylinderRadius;
            };
        };
#else
#error "FhRawCollisionVolume requires compiler support for anonymous struct members to preserve C# explicit-layout field names."
#endif

        Vector3Fx BoxVector3{};
        Fixed BoxDot1{};
        Fixed BoxDot2{};
        Fixed BoxDot3{};

        FhRawCollisionVolume() noexcept;
        FhRawCollisionVolume(const FhRawCollisionVolume& other) noexcept;
        FhRawCollisionVolume& operator=(const FhRawCollisionVolume& other) noexcept;
    };

    struct CameraSequenceHeader
    {
        const std::uint16_t Count = 0;
        const std::uint8_t Version = 0;
        const std::uint8_t Padding3 = 0;
        const std::uint32_t Padding4 = 0;

        CameraSequenceHeader() noexcept = default;
        CameraSequenceHeader(const CameraSequenceHeader&) noexcept = default;
        CameraSequenceHeader& operator=(const CameraSequenceHeader& other) noexcept;
    };

    struct RawCameraSequenceKeyframe
    {
        const Vector3Fx Position{};
        const Vector3Fx ToTarget{};
        const Fixed Roll{};
        const Fixed Fov{};
        const Fixed MoveTime{};
        const Fixed HoldTime{};
        const Fixed FadeInTime{};
        const Fixed FadeOutTime{};
        const FadeType FadeInType{};
        const FadeType FadeOutType{};
        const std::uint8_t PrevFrameInfluence = 0;
        const std::uint8_t AfterFrameInfluence = 0;
        const std::uint8_t UseEntityTransform = 0;
        const std::uint8_t Padding35 = 0;
        const std::uint16_t Padding36 = 0;
        const std::int16_t PosEntityType = 0;
        const std::int16_t PosEntityId = 0;
        const std::int16_t TargetEntityType = 0;
        const std::int16_t TargetEntityId = 0;
        const std::int16_t MessageTargetType = 0;
        const std::int16_t MessageTargetId = 0;
        const std::uint16_t MessageId = 0;
        const std::uint16_t MessageParam = 0;
        const Fixed Easing{};
        const std::uint32_t Unused4C = 0;
        const std::uint32_t Unused50 = 0;
        const std::array<std::uint8_t, 16> NodeName{};

        RawCameraSequenceKeyframe() noexcept = default;
        RawCameraSequenceKeyframe(const RawCameraSequenceKeyframe&) noexcept = default;
        RawCameraSequenceKeyframe& operator=(const RawCameraSequenceKeyframe& other) noexcept;

        [[nodiscard]] std::string NodeNameString() const;
    };

    struct RawEffect
    {
        const std::uint32_t Field0 = 0;
        const std::uint32_t FuncCount = 0;
        const std::uint32_t FuncOffset = 0;
        const std::uint32_t Count2 = 0;
        const std::uint32_t Offset2 = 0;
        const std::uint32_t ElementCount = 0;
        const std::uint32_t ElementOffset = 0;

        RawEffect() noexcept = default;
        RawEffect(const RawEffect&) noexcept = default;
        RawEffect& operator=(const RawEffect& other) noexcept;
    };

    struct RawEffectElement
    {
        const std::array<std::uint8_t, 32> Name{};
        const std::array<std::uint8_t, 32> ModelName{};
        const std::uint32_t ParticleCount = 0;
        const std::uint32_t ParticleOffset = 0;
        const Effects::EffElemFlags Flags{};
        const Vector3Fx Acceleration{};
        const std::uint32_t ChildEffectId = 0;
        const Fixed Lifespan{};
        const Fixed DrainTime{};
        const Fixed BufferTime{};
        const std::int32_t DrawType = 0;
        const std::uint32_t FuncCount = 0;
        const std::uint32_t FuncOffset = 0;

        RawEffectElement() noexcept = default;
        RawEffectElement(const RawEffectElement&) noexcept = default;
        RawEffectElement& operator=(const RawEffectElement& other) noexcept;

        [[nodiscard]] std::string NameString() const;
        [[nodiscard]] std::string ModelNameString() const;
    };

    struct RawStringTableEntry
    {
        const std::array<std::uint8_t, 4> Id{};
        const std::uint32_t Offset = 0;
        const std::uint16_t Length = 0;
        const std::uint8_t Speed = 0;
        const char Category = '\0';

        RawStringTableEntry() noexcept = default;
        RawStringTableEntry(const RawStringTableEntry&) noexcept = default;
        RawStringTableEntry& operator=(const RawStringTableEntry& other) noexcept;

        [[nodiscard]] std::string IdString() const;
    };

    struct TextFileEntry
    {
        const std::uint32_t Offset1 = 0;
        const std::uint32_t Offset2 = 0;
        const std::uint16_t Length1 = 0;
        const std::uint16_t Length2 = 0;

        TextFileEntry() noexcept = default;
        TextFileEntry(const TextFileEntry&) noexcept = default;
        TextFileEntry& operator=(const TextFileEntry& other) noexcept;
    };

    static_assert(sizeof(CullingMode) == 1);
    static_assert(sizeof(RepeatMode) == 1);
    static_assert(sizeof(RenderMode) == 1);
    static_assert(sizeof(TextureFormat) == 1);
    static_assert(sizeof(BillboardMode) == 1);
    static_assert(sizeof(FadeType) == 1);
    static_assert(sizeof(PolygonMode) == 4);
    static_assert(sizeof(TexgenMode) == 4);
    static_assert(sizeof(VolumeType) == 4);
    static_assert(sizeof(FhVolumeType) == 4);
    static_assert(sizeof(Effects::EffElemFlags) == 4);
    static_assert(sizeof(Fixed) == 4 && alignof(Fixed) == 4);
    static_assert(sizeof(Vector3Fx) == 12 && alignof(Vector3Fx) == 4);
    static_assert(sizeof(Matrix43Fx) == 48 && alignof(Matrix43Fx) == 4);
    static_assert(sizeof(ColorRgb) == 3 && alignof(ColorRgb) == 1);

    static_assert(std::is_standard_layout_v<RawMesh> && sizeof(RawMesh) == 4);
    static_assert(std::is_standard_layout_v<DisplayList> && sizeof(DisplayList) == 32);
    static_assert(std::is_standard_layout_v<RawMaterial> && sizeof(RawMaterial) == 132);
    static_assert(std::is_standard_layout_v<AnimationHeader> && sizeof(AnimationHeader) == 24);
    static_assert(std::is_standard_layout_v<RawMaterialAnimationGroup>
        && sizeof(RawMaterialAnimationGroup) == 20);
    static_assert(std::is_standard_layout_v<RawTextureAnimationGroup>
        && sizeof(RawTextureAnimationGroup) == 32);
    static_assert(std::is_standard_layout_v<RawTexcoordAnimationGroup>
        && sizeof(RawTexcoordAnimationGroup) == 28);
    static_assert(std::is_standard_layout_v<RawNodeAnimationGroup>
        && sizeof(RawNodeAnimationGroup) == 20);
    static_assert(std::is_standard_layout_v<MaterialAnimation>
        && sizeof(MaterialAnimation) == 140);
    static_assert(std::is_standard_layout_v<TextureAnimation>
        && sizeof(TextureAnimation) == 44);
    static_assert(std::is_standard_layout_v<TexcoordAnimation>
        && sizeof(TexcoordAnimation) == 60);
    static_assert(std::is_standard_layout_v<NodeAnimation> && sizeof(NodeAnimation) == 48);
    static_assert(std::is_standard_layout_v<Texture> && sizeof(Texture) == 40);
    static_assert(std::is_standard_layout_v<Palette> && sizeof(Palette) == 16);
    static_assert(std::is_standard_layout_v<Header> && sizeof(Header) == 100);
    static_assert(std::is_standard_layout_v<RawNode> && sizeof(RawNode) == 240);
    static_assert(std::is_standard_layout_v<RawCollisionVolume>
        && sizeof(RawCollisionVolume) == 64);
    static_assert(offsetof(RawCollisionVolume, Type) == 0);
    static_assert(offsetof(RawCollisionVolume, BoxVector1) == 4);
    static_assert(offsetof(RawCollisionVolume, BoxVector2) == 16);
    static_assert(offsetof(RawCollisionVolume, BoxVector3) == 28);
    static_assert(offsetof(RawCollisionVolume, BoxPosition) == 40);
    static_assert(offsetof(RawCollisionVolume, BoxDot1) == 52);
    static_assert(offsetof(RawCollisionVolume, BoxDot2) == 56);
    static_assert(offsetof(RawCollisionVolume, BoxDot3) == 60);
    static_assert(offsetof(RawCollisionVolume, CylinderVector) == 4);
    static_assert(offsetof(RawCollisionVolume, CylinderPosition) == 16);
    static_assert(offsetof(RawCollisionVolume, CylinderRadius) == 28);
    static_assert(offsetof(RawCollisionVolume, CylinderDot) == 32);
    static_assert(offsetof(RawCollisionVolume, SpherePosition) == 4);
    static_assert(offsetof(RawCollisionVolume, SphereRadius) == 16);
    static_assert(std::is_standard_layout_v<FhRawCollisionVolume>
        && sizeof(FhRawCollisionVolume) == 64);
    static_assert(offsetof(FhRawCollisionVolume, Type) == 0);
    static_assert(offsetof(FhRawCollisionVolume, BoxPosition) == 4);
    static_assert(offsetof(FhRawCollisionVolume, BoxVector1) == 16);
    static_assert(offsetof(FhRawCollisionVolume, BoxVector2) == 28);
    static_assert(offsetof(FhRawCollisionVolume, BoxVector3) == 40);
    static_assert(offsetof(FhRawCollisionVolume, BoxDot1) == 52);
    static_assert(offsetof(FhRawCollisionVolume, BoxDot2) == 56);
    static_assert(offsetof(FhRawCollisionVolume, BoxDot3) == 60);
    static_assert(offsetof(FhRawCollisionVolume, CylinderPosition) == 4);
    static_assert(offsetof(FhRawCollisionVolume, CylinderVector) == 16);
    static_assert(offsetof(FhRawCollisionVolume, CylinderDot) == 28);
    static_assert(offsetof(FhRawCollisionVolume, CylinderRadius) == 32);
    static_assert(offsetof(FhRawCollisionVolume, SpherePosition) == 4);
    static_assert(offsetof(FhRawCollisionVolume, SphereRadius) == 16);
    static_assert(std::is_standard_layout_v<CameraSequenceHeader>
        && sizeof(CameraSequenceHeader) == 8);
    static_assert(std::is_standard_layout_v<RawCameraSequenceKeyframe>
        && sizeof(RawCameraSequenceKeyframe) == 100);
    static_assert(std::is_standard_layout_v<RawEffect> && sizeof(RawEffect) == 28);
    static_assert(std::is_standard_layout_v<RawEffectElement>
        && sizeof(RawEffectElement) == 116);
    static_assert(std::is_standard_layout_v<RawStringTableEntry>
        && sizeof(RawStringTableEntry) == 12);
    static_assert(offsetof(RawStringTableEntry, Id) == 0);
    static_assert(offsetof(RawStringTableEntry, Offset) == 4);
    static_assert(offsetof(RawStringTableEntry, Length) == 8);
    static_assert(offsetof(RawStringTableEntry, Speed) == 10);
    static_assert(offsetof(RawStringTableEntry, Category) == 11);
    static_assert(std::is_standard_layout_v<TextFileEntry> && sizeof(TextFileEntry) == 12);
}
