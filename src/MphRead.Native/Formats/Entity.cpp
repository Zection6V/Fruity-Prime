#include "Entity.hpp"

#include <algorithm>
#include <memory>
#include <new>
#include <stdexcept>

namespace
{
    template <typename T>
    T& AssignReadonly(T& self, const T& other) noexcept
    {
        if (std::addressof(self) != std::addressof(other))
        {
            self.~T();
            ::new (static_cast<void*>(std::addressof(self))) T(other);
        }
        return self;
    }

    template <std::size_t N>
    void CopyExact(std::optional<std::string_view> source, char (&destination)[N])
    {
        if (!source.has_value())
        {
            return;
        }
        if (source->size() > N)
        {
            // System.String.CopyTo(Span<char>) throws ArgumentException when
            // the destination is shorter than the source.
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source->begin(), source->end(), destination);
    }

    template <std::size_t N>
    void CopyTruncated(std::optional<std::string_view> source, char (&destination)[N]) noexcept
    {
        if (!source.has_value())
        {
            return;
        }
        const std::size_t count = std::min(N, source->size());
        std::copy_n(source->begin(), count, destination);
    }

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }
}

namespace MphRead
{
    EntityDataHeader::EntityDataHeader(std::uint16_t type, std::int16_t entityId,
        OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 upVector,
        OpenTK::Mathematics::Vector3 facingVector) noexcept
        : Type(type),
          EntityId(entityId),
          Position(TypeExtensions::ToVector3Fx(position)),
          UpVector(TypeExtensions::ToVector3Fx(upVector)),
          FacingVector(TypeExtensions::ToVector3Fx(facingVector))
    {
    }

    DoorEntityData::DoorEntityData(EntityDataHeader header,
        std::optional<std::string_view> nodeName,
        std::uint32_t paletteId,
        MphRead::DoorType doorType,
        std::uint32_t connectorId,
        std::uint8_t targetLayerId,
        std::uint8_t locked,
        std::uint8_t outConnectorId,
        std::uint8_t outLoaderId,
        std::optional<std::string_view> entityFilename,
        std::optional<std::string_view> roomName)
        : Header(header),
          PaletteId(paletteId),
          DoorType(doorType),
          ConnectorId(connectorId),
          TargetLayerId(targetLayerId),
          Locked(locked),
          OutConnectorId(outConnectorId),
          OutLoaderId(outLoaderId)
    {
        CopyExact(nodeName, NodeName);
        CopyExact(entityFilename, EntityFilename);
        CopyExact(roomName, RoomName);
    }

    FhRawCollisionVolume FhTriggerVolumeEntityData::ActiveVolume() const noexcept
    {
        if (Subtype == FhTriggerType::Cylinder)
        {
            return Cylinder;
        }
        if (Subtype == FhTriggerType::Box)
        {
            return Box;
        }
        return Sphere;
    }

    FhRawCollisionVolume FhAreaVolumeEntityData::ActiveVolume() const noexcept
    {
        if (Subtype == FhTriggerType::Cylinder)
        {
            return Cylinder;
        }
        if (Subtype == FhTriggerType::Box)
        {
            return Box;
        }
        return Sphere;
    }

    JumpPadEntityData::JumpPadEntityData(EntityDataHeader header,
        std::int32_t parentId,
        RawCollisionVolume volume,
        Vector3Fx beamVector,
        Fixed speed,
        std::uint16_t controlLockTime,
        std::uint16_t cooldownTime,
        std::uint8_t active,
        std::uint32_t modelId,
        std::uint32_t beamType,
        Entities::TriggerFlags triggerFlags) noexcept
        : Header(header),
          ParentId(parentId),
          Volume(volume),
          BeamVector(beamVector),
          Speed(speed),
          ControlLockTime(controlLockTime),
          CooldownTime(cooldownTime),
          Active(active),
          ModelId(modelId),
          BeamType(beamType),
          TriggerFlags(triggerFlags)
    {
        // Unused28 and padding intentionally retain default zero.
    }

    FhRawCollisionVolume FhJumpPadEntityData::ActiveVolume() const noexcept
    {
        if (VolumeType == FhTriggerType::Sphere)
        {
            return Sphere;
        }
        if (VolumeType == FhTriggerType::Box)
        {
            return Box;
        }
        if (VolumeType == FhTriggerType::Cylinder)
        {
            return Cylinder;
        }
        return FhRawCollisionVolume{};
    }

    TeleporterEntityData::TeleporterEntityData(EntityDataHeader header,
        std::uint8_t loadIndex,
        std::uint8_t targetIndex,
        std::uint8_t artifactId,
        std::uint8_t active,
        std::uint8_t invisible,
        std::optional<std::string_view> entityFilename,
        Vector3Fx targetPosition,
        std::optional<std::string_view> nodeName) noexcept
        : Header(header),
          LoadIndex(loadIndex),
          TargetIndex(targetIndex),
          ArtifactId(artifactId),
          Active(active),
          Invisible(invisible),
          Unused3A(UINT16_MAX),
          TargetPosition(targetPosition)
    {
        CopyTruncated(entityFilename, EntityFilename);
        CopyTruncated(nodeName, NodeName);
        // Unused38 intentionally retains default zero.
    }

    ArtifactEntityData::ArtifactEntityData(EntityDataHeader header,
        std::uint8_t modelId,
        std::uint8_t artifactId,
        std::uint8_t active,
        std::uint8_t hasBase,
        std::int16_t message1Target,
        Message message1,
        std::int16_t message2Target,
        Message message2,
        std::int16_t message3Target,
        Message message3,
        std::int16_t linkedEntityId) noexcept
        : Header(header),
          ModelId(modelId),
          ArtifactId(artifactId),
          Active(active),
          HasBase(hasBase),
          Message1Target(message1Target),
          Message1(message1),
          Message2Target(message2Target),
          Message2(message2),
          Message3Target(message3Target),
          Message3(message3),
          LinkedEntityId(linkedEntityId)
    {
        // Padding2A/Padding32/Padding3A intentionally retain default zero.
    }

    Vector3Fx Vector3FxArray8::operator[](std::int32_t index) const
    {
        switch (index)
        {
        case 0: return Vector0;
        case 1: return Vector1;
        case 2: return Vector2;
        case 3: return Vector3;
        case 4: return Vector4;
        case 5: return Vector5;
        case 6: return Vector6;
        case 7: return Vector7;
        default: ThrowIndexOutOfRange();
        }
    }

    Vector3Fx Vector3FxArray10::operator[](std::int32_t index) const
    {
        switch (index)
        {
        case 0: return Vector0;
        case 1: return Vector1;
        case 2: return Vector2;
        case 3: return Vector3;
        case 4: return Vector4;
        case 5: return Vector5;
        case 6: return Vector6;
        case 7: return Vector7;
        case 8: return Vector8;
        case 9: return Vector9;
        default: ThrowIndexOutOfRange();
        }
    }

    Vector4Fx Vector4FxArray10::operator[](std::int32_t index) const
    {
        switch (index)
        {
        case 0: return Vector0;
        case 1: return Vector1;
        case 2: return Vector2;
        case 3: return Vector3;
        case 4: return Vector4;
        case 5: return Vector5;
        case 6: return Vector6;
        case 7: return Vector7;
        case 8: return Vector8;
        case 9: return Vector9;
        default: ThrowIndexOutOfRange();
        }
    }

    Vector3Fx Vector3FxArray16::operator[](std::int32_t index) const
    {
        switch (index)
        {
        case 0: return Vector00;
        case 1: return Vector01;
        case 2: return Vector02;
        case 3: return Vector03;
        case 4: return Vector04;
        case 5: return Vector05;
        case 6: return Vector06;
        case 7: return Vector07;
        case 8: return Vector08;
        case 9: return Vector09;
        case 10: return Vector10;
        case 11: return Vector11;
        case 12: return Vector12;
        case 13: return Vector13;
        case 14: return Vector14;
        case 15: return Vector15;
        default: ThrowIndexOutOfRange();
        }
    }

    std::uint16_t EntityLengthArray::operator[](std::int32_t index) const
    {
        switch (index)
        {
        case 0: return Length00;
        case 1: return Length01;
        case 2: return Length02;
        case 3: return Length03;
        case 4: return Length04;
        case 5: return Length05;
        case 6: return Length06;
        case 7: return Length07;
        case 8: return Length08;
        case 9: return Length09;
        case 10: return Length10;
        case 11: return Length11;
        case 12: return Length12;
        case 13: return Length13;
        case 14: return Length14;
        case 15: return Length15;
        default: ThrowIndexOutOfRange();
        }
    }

#define MPHREAD_DEFINE_READONLY_ASSIGNMENT(TypeName) \
    TypeName& TypeName::operator=(const TypeName& other) noexcept \
    { \
        return AssignReadonly(*this, other); \
    }

    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Vector3FxArray8)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Vector3FxArray10)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Vector4FxArray10)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(Vector3FxArray16)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(EntityLengthArray)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(EntityHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(EntityEntry)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhEntityEntry)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(EntityDataHeader)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(PlatformEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhPlatformEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(ObjectEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(PlayerSpawnEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(DoorEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhDoorEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(ItemSpawnEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhItemSpawnEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TriggerVolumeEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhTriggerVolumeEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(AreaVolumeEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhAreaVolumeEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(JumpPadEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhJumpPadEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(PointModuleEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(MorphCameraEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FhMorphCameraEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(OctolithFlagEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(FlagBaseEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(TeleporterEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(NodeDefenseEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(LightSourceEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(ArtifactEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(CameraSequenceEntityData)
    MPHREAD_DEFINE_READONLY_ASSIGNMENT(ForceFieldEntityData)

#undef MPHREAD_DEFINE_READONLY_ASSIGNMENT
}
