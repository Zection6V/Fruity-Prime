#pragma once

#include "Enums.hpp"
#include "RawFormats.hpp"
#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace MphRead::Entities
{
    // Exact external dependency owners from the C# MphRead.Entities namespace.
    // Values remain owned by their one-to-one Native entity counterparts.
    enum class PlatformFlags : std::uint32_t;
    enum class ObjectFlags : std::uint8_t;
    enum class ObjEffFlags : std::uint32_t;
    enum class TriggerFlags : std::uint32_t;
    enum class FhTriggerFlags : std::uint32_t;
}

namespace MphRead
{
    // Forward declarations preserve the C# declaration order. The fixed inline
    // array wrapper definitions are placed before their first value use below
    // because C++ requires complete member types.
    struct EntityHeader;
    struct EntityEntry;
    struct FhEntityEntry;
    struct EntityDataHeader;
    struct PlatformEntityData;
    struct FhPlatformEntityData;
    struct ObjectEntityData;
    struct PlayerSpawnEntityData;
    struct DoorEntityData;
    struct FhDoorEntityData;
    struct ItemSpawnEntityData;
    struct FhItemSpawnEntityData;
    struct TriggerVolumeEntityData;
    struct FhTriggerVolumeEntityData;
    struct AreaVolumeEntityData;
    struct FhAreaVolumeEntityData;
    struct JumpPadEntityData;
    struct FhJumpPadEntityData;
    struct PointModuleEntityData;
    struct MorphCameraEntityData;
    struct FhMorphCameraEntityData;
    struct OctolithFlagEntityData;
    struct FlagBaseEntityData;
    struct TeleporterEntityData;
    struct NodeDefenseEntityData;
    struct LightSourceEntityData;
    struct ArtifactEntityData;
    struct CameraSequenceEntityData;
    struct ForceFieldEntityData;
    struct Vector3FxArray8;
    struct Vector3FxArray10;
    struct Vector4FxArray10;
    struct Vector3FxArray16;
    struct EntityLengthArray;

    // size: 96 (12 x 8)
    struct Vector3FxArray8
    {
        const Vector3Fx Vector0{};
        const Vector3Fx Vector1{};
        const Vector3Fx Vector2{};
        const Vector3Fx Vector3{};
        const Vector3Fx Vector4{};
        const Vector3Fx Vector5{};
        const Vector3Fx Vector6{};
        const Vector3Fx Vector7{};

        constexpr Vector3FxArray8() noexcept = default;
        Vector3FxArray8(const Vector3FxArray8&) noexcept = default;
        Vector3FxArray8& operator=(const Vector3FxArray8& other) noexcept;
        [[nodiscard]] Vector3Fx operator[](std::int32_t index) const;
    };

    // size: 120 (12 x 10)
    struct Vector3FxArray10
    {
        const Vector3Fx Vector0{};
        const Vector3Fx Vector1{};
        const Vector3Fx Vector2{};
        const Vector3Fx Vector3{};
        const Vector3Fx Vector4{};
        const Vector3Fx Vector5{};
        const Vector3Fx Vector6{};
        const Vector3Fx Vector7{};
        const Vector3Fx Vector8{};
        const Vector3Fx Vector9{};

        constexpr Vector3FxArray10() noexcept = default;
        Vector3FxArray10(const Vector3FxArray10&) noexcept = default;
        Vector3FxArray10& operator=(const Vector3FxArray10& other) noexcept;
        [[nodiscard]] Vector3Fx operator[](std::int32_t index) const;
    };

    // size: 160 (16 x 10)
    struct Vector4FxArray10
    {
        const Vector4Fx Vector0{};
        const Vector4Fx Vector1{};
        const Vector4Fx Vector2{};
        const Vector4Fx Vector3{};
        const Vector4Fx Vector4{};
        const Vector4Fx Vector5{};
        const Vector4Fx Vector6{};
        const Vector4Fx Vector7{};
        const Vector4Fx Vector8{};
        const Vector4Fx Vector9{};

        constexpr Vector4FxArray10() noexcept = default;
        Vector4FxArray10(const Vector4FxArray10&) noexcept = default;
        Vector4FxArray10& operator=(const Vector4FxArray10& other) noexcept;
        [[nodiscard]] Vector4Fx operator[](std::int32_t index) const;
    };

    // C# source comment: size: 160 (12 x 16)
    // Sixteen 12-byte Vector3Fx fields actually occupy 192 bytes.
    struct Vector3FxArray16
    {
        const Vector3Fx Vector00{};
        const Vector3Fx Vector01{};
        const Vector3Fx Vector02{};
        const Vector3Fx Vector03{};
        const Vector3Fx Vector04{};
        const Vector3Fx Vector05{};
        const Vector3Fx Vector06{};
        const Vector3Fx Vector07{};
        const Vector3Fx Vector08{};
        const Vector3Fx Vector09{};
        const Vector3Fx Vector10{};
        const Vector3Fx Vector11{};
        const Vector3Fx Vector12{};
        const Vector3Fx Vector13{};
        const Vector3Fx Vector14{};
        const Vector3Fx Vector15{};

        constexpr Vector3FxArray16() noexcept = default;
        Vector3FxArray16(const Vector3FxArray16&) noexcept = default;
        Vector3FxArray16& operator=(const Vector3FxArray16& other) noexcept;
        [[nodiscard]] Vector3Fx operator[](std::int32_t index) const;
    };

    // size: 32 (2 x 16)
    struct EntityLengthArray
    {
        const std::uint16_t Length00 = 0;
        const std::uint16_t Length01 = 0;
        const std::uint16_t Length02 = 0;
        const std::uint16_t Length03 = 0;
        const std::uint16_t Length04 = 0;
        const std::uint16_t Length05 = 0;
        const std::uint16_t Length06 = 0;
        const std::uint16_t Length07 = 0;
        const std::uint16_t Length08 = 0;
        const std::uint16_t Length09 = 0;
        const std::uint16_t Length10 = 0;
        const std::uint16_t Length11 = 0;
        const std::uint16_t Length12 = 0;
        const std::uint16_t Length13 = 0;
        const std::uint16_t Length14 = 0;
        const std::uint16_t Length15 = 0;

        constexpr EntityLengthArray() noexcept = default;
        EntityLengthArray(const EntityLengthArray&) noexcept = default;
        EntityLengthArray& operator=(const EntityLengthArray& other) noexcept;
        [[nodiscard]] std::uint16_t operator[](std::int32_t index) const;
    };

    // size: 36
    struct EntityHeader
    {
        const std::uint32_t Version = 0;
        // putting lengths on a separate struct so we can index e.g. header.Lengths[0]
        const EntityLengthArray Lengths{};

        constexpr EntityHeader() noexcept = default;
        EntityHeader(const EntityHeader&) noexcept = default;
        EntityHeader& operator=(const EntityHeader& other) noexcept;
    };

    // size: 24
    struct EntityEntry
    {
        // C# [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] char[].
        // Default StructLayout CharSet marshals each element as one ANSI byte.
        char NodeName[16]{}; // todo: use this for partial room visibility
        const std::uint16_t LayerMask = 0;
        const std::uint16_t Length = 0;
        const std::uint32_t DataOffset = 0;

        constexpr EntityEntry() noexcept = default;
        EntityEntry(const EntityEntry&) noexcept = default;
        EntityEntry& operator=(const EntityEntry& other) noexcept;
    };

    // size: 20
    struct FhEntityEntry
    {
        char NodeName[16]{}; // todo: same as above
        const std::uint32_t DataOffset = 0;

        constexpr FhEntityEntry() noexcept = default;
        FhEntityEntry(const FhEntityEntry&) noexcept = default;
        FhEntityEntry& operator=(const FhEntityEntry& other) noexcept;
    };

    // size: 40
    struct EntityDataHeader
    {
        const std::uint16_t Type = 0;
        const std::int16_t EntityId = 0; // counts up
        const Vector3Fx Position{};
        const Vector3Fx UpVector{};
        const Vector3Fx FacingVector{};

        constexpr EntityDataHeader() noexcept = default;
        EntityDataHeader(std::uint16_t type, std::int16_t entityId,
            OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 upVector,
            OpenTK::Mathematics::Vector3 facingVector) noexcept;
        EntityDataHeader(const EntityDataHeader&) noexcept = default;
        EntityDataHeader& operator=(const EntityDataHeader& other) noexcept;
    };

    // size: 588
    struct PlatformEntityData
    {
        const EntityDataHeader Header{};
        const std::uint32_t NoPort = 0;
        const std::uint32_t ModelId = 0;
        const std::int16_t ParentId = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t Delay = 0;
        const std::uint16_t ScanData1 = 0;
        const std::int16_t ScanMsgTarget = 0;
        const Message ScanMessage{};
        const std::uint16_t ScanData2 = 0;
        const std::uint16_t PositionCount = 0;
        const Vector3FxArray10 Positions{};
        const Vector4FxArray10 Rotations{};
        const Vector3Fx PositionOffset{};
        const Fixed ForwardSpeed{};
        const Fixed BackwardSpeed{};
        char PortalName[16]{};
        const std::uint32_t MovementType = 0;
        const std::uint32_t ForCutscene = 0;
        const std::uint32_t ReverseType = 0;
        const Entities::PlatformFlags Flags{};
        const std::uint32_t ContactDamage = 0;
        const Vector3Fx BeamSpawnDir{};
        const Vector3Fx BeamSpawnPos{};
        const std::int32_t BeamId = 0;
        const std::uint32_t BeamInterval = 0;
        const std::uint32_t BeamOnIntervals = 0;
        const std::uint16_t Unused1B0 = 0;
        const std::uint16_t Unused1B2 = 0;
        const std::int32_t ResistEffectId = 0;
        const std::uint32_t Health = 0;
        const std::uint32_t Effectiveness = 0;
        const std::int32_t DamageEffectId = 0;
        const std::int32_t DeadEffectId = 0;
        const std::uint8_t ItemChance = 0;
        const std::uint8_t Padding1C9 = 0;
        const std::uint16_t Padding1CA = 0;
        const MphRead::ItemType ItemType{};
        const std::uint32_t Unused1D0 = 0;
        const std::uint32_t Unused1D4 = 0;
        const std::int32_t BeamHitMsgTarget = 0;
        const Message BeamHitMessage{};
        const std::int32_t BeamHitMsgParam1 = 0;
        const std::int32_t BeamHitMsgParam2 = 0;
        const std::int32_t PlayerColMsgTarget = 0;
        const Message PlayerColMessage{};
        const std::int32_t PlayerColMsgParam1 = 0;
        const std::int32_t PlayerColMsgParam2 = 0;
        const std::int32_t DeadMsgTarget = 0;
        const Message DeadMessage{};
        const std::int32_t DeadMsgParam1 = 0;
        const std::int32_t DeadMsgParam2 = 0;
        const std::uint16_t LifetimeMsg1Index = 0;
        const std::int16_t LifetimeMsg1Target = 0;
        const Message LifetimeMessage1{};
        const std::int32_t LifetimeMsg1Param1 = 0;
        const std::int32_t LifetimeMsg1Param2 = 0;
        const std::uint16_t LifetimeMsg2Index = 0;
        const std::int16_t LifetimeMsg2Target = 0;
        const Message LifetimeMessage2{};
        const std::int32_t LifetimeMsg2Param1 = 0;
        const std::int32_t LifetimeMsg2Param2 = 0;
        const std::uint16_t LifetimeMsg3Index = 0;
        const std::int16_t LifetimeMsg3Target = 0;
        const Message LifetimeMessage3{};
        const std::int32_t LifetimeMsg3Param1 = 0;
        const std::int32_t LifetimeMsg3Param2 = 0;
        const std::uint16_t LifetimeMsg4Index = 0;
        const std::int16_t LifetimeMsg4Target = 0;
        const Message LifetimeMessage4{};
        const std::int32_t LifetimeMsg4Param1 = 0;
        const std::int32_t LifetimeMsg4Param2 = 0;

        constexpr PlatformEntityData() noexcept = default;
        PlatformEntityData(const PlatformEntityData&) noexcept = default;
        PlatformEntityData& operator=(const PlatformEntityData& other) noexcept;
    };

    // size: 236
    struct FhPlatformEntityData
    {
        const EntityDataHeader Header{};
        const std::uint32_t NoPortal = 0;
        const std::uint32_t GroupId = 0;
        const std::uint32_t Unused2C = 0;
        const std::uint8_t Delay = 0;
        const std::uint8_t PositionCount = 0;
        const std::uint16_t Padding32 = 0;
        const FhRawCollisionVolume Volume{};
        const Vector3FxArray8 Positions{};
        const Fixed Speed{};
        char PortalName[16]{};

        constexpr FhPlatformEntityData() noexcept = default;
        FhPlatformEntityData(const FhPlatformEntityData&) noexcept = default;
        FhPlatformEntityData& operator=(const FhPlatformEntityData& other) noexcept;
    };

    // size: 152
    struct ObjectEntityData
    {
        const EntityDataHeader Header{};
        const Entities::ObjectFlags Flags{};
        const std::uint8_t Padding25 = 0;
        const std::uint16_t Padding26 = 0;
        const Entities::ObjEffFlags EffectFlags{};
        const std::int32_t ModelId = 0;
        const std::int16_t LinkedEntity = 0;
        const std::uint16_t ScanId = 0;
        const std::int16_t ScanMsgTarget = 0;
        const std::uint16_t Padding36 = 0;
        const Message ScanMessage{};
        const std::int32_t EffectId = 0;
        const std::uint32_t EffectInterval = 0;
        const std::uint32_t EffectOnIntervals = 0;
        const Vector3Fx EffectPositionOffset{};
        const RawCollisionVolume Volume{};

        constexpr ObjectEntityData() noexcept = default;
        ObjectEntityData(const ObjectEntityData&) noexcept = default;
        ObjectEntityData& operator=(const ObjectEntityData& other) noexcept;
    };

#pragma pack(push, 1)
    // size: 43
    struct PlayerSpawnEntityData
    {
        const EntityDataHeader Header{};
        const std::uint8_t Availability = 0;
        const std::uint8_t Active = 0;
        const std::int8_t TeamIndex = 0;

        constexpr PlayerSpawnEntityData() noexcept = default;
        PlayerSpawnEntityData(const PlayerSpawnEntityData&) noexcept = default;
        PlayerSpawnEntityData& operator=(const PlayerSpawnEntityData& other) noexcept;
    };
#pragma pack(pop)

    // size: 104
    struct DoorEntityData
    {
        const EntityDataHeader Header{};
        char NodeName[16]{};
        const std::uint32_t PaletteId = 0;
        const MphRead::DoorType DoorType{};
        const std::uint32_t ConnectorId = 0;
        const std::uint8_t TargetLayerId = 0;
        const std::uint8_t Locked = 0;
        const std::uint8_t OutConnectorId = 0;
        const std::uint8_t OutLoaderId = 0;
        char EntityFilename[16]{};
        char RoomName[16]{};

        constexpr DoorEntityData() noexcept = default;
        DoorEntityData(EntityDataHeader header, std::optional<std::string_view> nodeName,
            std::uint32_t paletteId, MphRead::DoorType doorType, std::uint32_t connectorId,
            std::uint8_t targetLayerId, std::uint8_t locked, std::uint8_t outConnectorId,
            std::uint8_t outLoaderId, std::optional<std::string_view> entityFilename,
            std::optional<std::string_view> roomName);
        DoorEntityData(const DoorEntityData&) noexcept = default;
        DoorEntityData& operator=(const DoorEntityData& other) noexcept;
    };

    // size: 64
    struct FhDoorEntityData
    {
        const EntityDataHeader Header{};
        char RoomName[16]{};
        const std::uint32_t Locked = 0;
        const std::uint32_t ModelId = 0;

        constexpr FhDoorEntityData() noexcept = default;
        FhDoorEntityData(const FhDoorEntityData&) noexcept = default;
        FhDoorEntityData& operator=(const FhDoorEntityData& other) noexcept;
    };

    // size: 72
    struct ItemSpawnEntityData
    {
        const EntityDataHeader Header{};
        const std::int32_t ParentId = 0;
        const MphRead::ItemType ItemType{};
        const std::uint8_t Enabled = 0;
        const std::uint8_t HasBase = 0;
        const std::uint8_t AlwaysActive = 0;
        const std::uint8_t Padding2F = 0;
        const std::uint16_t MaxSpawnCount = 0;
        const std::uint16_t SpawnInterval = 0;
        const std::uint16_t SpawnDelay = 0;
        const std::int16_t NotifyEntityId = 0;
        const Message CollectedMessage{};
        const std::int32_t CollectedMsgParam1 = 0;
        const std::int32_t CollectedMsgParam2 = 0;

        constexpr ItemSpawnEntityData() noexcept = default;
        ItemSpawnEntityData(const ItemSpawnEntityData&) noexcept = default;
        ItemSpawnEntityData& operator=(const ItemSpawnEntityData& other) noexcept;
    };

#pragma pack(push, 1)
    // size: 50
    struct FhItemSpawnEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::FhItemType ItemType{};
        const std::uint16_t SpawnLimit = 0;
        const std::uint16_t CooldownTime = 0;
        const std::uint16_t Unused2C = 0;

        constexpr FhItemSpawnEntityData() noexcept = default;
        FhItemSpawnEntityData(const FhItemSpawnEntityData&) noexcept = default;
        FhItemSpawnEntityData& operator=(const FhItemSpawnEntityData& other) noexcept;
    };
#pragma pack(pop)

    // size: 160
    struct TriggerVolumeEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::TriggerType Subtype{};
        const RawCollisionVolume Volume{};
        const std::uint16_t Unused68 = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t AlwaysActive = 0;
        const std::uint8_t DeactivateAfterUse = 0;
        const std::uint8_t Padding6D = 0;
        const std::uint16_t RepeatDelay = 0;
        const std::uint16_t CheckDelay = 0;
        const std::uint16_t RequiredStateBit = 0;
        const Entities::TriggerFlags TriggerFlags{};
        const std::uint32_t TriggerThreshold = 0;
        const std::int16_t ParentId = 0;
        const std::uint16_t Padding7E = 0;
        const Message ParentMessage{};
        const std::int32_t ParentMsgParam1 = 0;
        const std::int32_t ParentMsgParam2 = 0;
        const std::int16_t ChildId = 0;
        const std::uint16_t Padding8E = 0;
        const Message ChildMessage{};
        const std::int32_t ChildMsgParam1 = 0;
        const std::int32_t ChildMsgParam2 = 0;

        constexpr TriggerVolumeEntityData() noexcept = default;
        TriggerVolumeEntityData(const TriggerVolumeEntityData&) noexcept = default;
        TriggerVolumeEntityData& operator=(const TriggerVolumeEntityData& other) noexcept;
    };

    // size: 272
    struct FhTriggerVolumeEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::FhTriggerType Subtype{};
        const FhRawCollisionVolume Box{};
        const FhRawCollisionVolume Sphere{};
        const FhRawCollisionVolume Cylinder{};
        const std::uint16_t OneUse = 0;
        const std::uint16_t Cooldown = 0;
        const Entities::FhTriggerFlags TriggerFlags{};
        const std::uint32_t Threshold = 0;
        const std::int16_t ParentId = 0;
        const std::uint16_t PaddingF6 = 0;
        const FhMessage ParentMessage{};
        const std::int32_t ParentMsgParam1 = 0;
        const std::int16_t ChildId = 0;
        const std::uint16_t Padding102 = 0;
        const FhMessage ChildMessage{};
        const std::int32_t ChildMsgParam1 = 0;

        constexpr FhTriggerVolumeEntityData() noexcept = default;
        FhTriggerVolumeEntityData(const FhTriggerVolumeEntityData&) noexcept = default;
        FhTriggerVolumeEntityData& operator=(const FhTriggerVolumeEntityData& other) noexcept;
        [[nodiscard]] FhRawCollisionVolume ActiveVolume() const noexcept;
    };

    // size: 152
    struct AreaVolumeEntityData
    {
        const EntityDataHeader Header{};
        const RawCollisionVolume Volume{};
        const std::uint16_t Unused64 = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t AlwaysActive = 0;
        const std::uint8_t AllowMultiple = 0;
        const std::uint8_t MessageDelay = 0;
        const std::uint16_t Unused6A = 0;
        const Message InsideMessage{};
        const std::int32_t InsideMsgParam1 = 0;
        const std::int32_t InsideMsgParam2 = 0;
        const std::int16_t ParentId = 0;
        const std::uint16_t Padding7A = 0;
        const Message ExitMessage{};
        const std::int32_t ExitMsgParam1 = 0;
        const std::int32_t ExitMsgParam2 = 0;
        const std::int16_t ChildId = 0;
        const std::uint16_t Cooldown = 0;
        const std::uint32_t Priority = 0;
        const Entities::TriggerFlags TriggerFlags{};

        constexpr AreaVolumeEntityData() noexcept = default;
        AreaVolumeEntityData(const AreaVolumeEntityData&) noexcept = default;
        AreaVolumeEntityData& operator=(const AreaVolumeEntityData& other) noexcept;
    };

    // size: 260
    struct FhAreaVolumeEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::FhTriggerType Subtype{};
        const FhRawCollisionVolume Box{};
        const FhRawCollisionVolume Sphere{};
        const FhRawCollisionVolume Cylinder{};
        const FhMessage InsideMessage{};
        const std::int32_t InsideMsgParam1 = 0;
        const FhMessage ExitMessage{};
        const std::int32_t ExitMsgParam1 = 0;
        const std::uint16_t Cooldown = 0;
        const std::uint16_t PaddingFA = 0;
        const Entities::FhTriggerFlags TriggerFlags{};

        constexpr FhAreaVolumeEntityData() noexcept = default;
        FhAreaVolumeEntityData(const FhAreaVolumeEntityData&) noexcept = default;
        FhAreaVolumeEntityData& operator=(const FhAreaVolumeEntityData& other) noexcept;
        [[nodiscard]] FhRawCollisionVolume ActiveVolume() const noexcept;
    };

    // size: 148
    struct JumpPadEntityData
    {
        const EntityDataHeader Header{};
        const std::int32_t ParentId = 0;
        const std::uint32_t Unused28 = 0;
        const RawCollisionVolume Volume{};
        const Vector3Fx BeamVector{};
        const Fixed Speed{};
        const std::uint16_t ControlLockTime = 0;
        const std::uint16_t CooldownTime = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t Padding81 = 0;
        const std::uint16_t Padding82 = 0;
        const std::uint32_t ModelId = 0;
        const std::uint32_t BeamType = 0;
        const Entities::TriggerFlags TriggerFlags{};

        constexpr JumpPadEntityData() noexcept = default;
        JumpPadEntityData(EntityDataHeader header, std::int32_t parentId,
            RawCollisionVolume volume, Vector3Fx beamVector, Fixed speed,
            std::uint16_t controlLockTime, std::uint16_t cooldownTime,
            std::uint8_t active, std::uint32_t modelId, std::uint32_t beamType,
            Entities::TriggerFlags triggerFlags) noexcept;
        JumpPadEntityData(const JumpPadEntityData&) noexcept = default;
        JumpPadEntityData& operator=(const JumpPadEntityData& other) noexcept;
    };

    // size: 272
    struct FhJumpPadEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::FhTriggerType VolumeType{};
        const FhRawCollisionVolume Box{};
        const FhRawCollisionVolume Sphere{};
        const FhRawCollisionVolume Cylinder{};
        const std::uint32_t CooldownTime = 0;
        const Vector3Fx BeamVector{};
        const Fixed Speed{};
        const std::uint32_t ControlLockTime = 0;
        const std::uint32_t ModelId = 0;
        const std::uint32_t BeamType = 0;
        const Entities::FhTriggerFlags TriggerFlags{};

        constexpr FhJumpPadEntityData() noexcept = default;
        FhJumpPadEntityData(const FhJumpPadEntityData&) noexcept = default;
        FhJumpPadEntityData& operator=(const FhJumpPadEntityData& other) noexcept;
        [[nodiscard]] FhRawCollisionVolume ActiveVolume() const noexcept;
    };

#pragma pack(push, 1)
    // size: 45
    struct PointModuleEntityData
    {
        const EntityDataHeader Header{};
        const std::int16_t NextId = 0;
        const std::int16_t PrevId = 0;
        const std::uint8_t Active = 0;

        constexpr PointModuleEntityData() noexcept = default;
        PointModuleEntityData(const PointModuleEntityData&) noexcept = default;
        PointModuleEntityData& operator=(const PointModuleEntityData& other) noexcept;
    };
#pragma pack(pop)

    // size: 104
    struct MorphCameraEntityData
    {
        const EntityDataHeader Header{};
        const RawCollisionVolume Volume{};

        constexpr MorphCameraEntityData() noexcept = default;
        MorphCameraEntityData(const MorphCameraEntityData&) noexcept = default;
        MorphCameraEntityData& operator=(const MorphCameraEntityData& other) noexcept;
    };

    // size: 104
    struct FhMorphCameraEntityData
    {
        const EntityDataHeader Header{};
        const FhRawCollisionVolume Volume{};

        constexpr FhMorphCameraEntityData() noexcept = default;
        FhMorphCameraEntityData(const FhMorphCameraEntityData&) noexcept = default;
        FhMorphCameraEntityData& operator=(const FhMorphCameraEntityData& other) noexcept;
    };

#pragma pack(push, 1)
    // size: 41
    struct OctolithFlagEntityData
    {
        const EntityDataHeader Header{};
        const std::uint8_t TeamId = 0;

        constexpr OctolithFlagEntityData() noexcept = default;
        OctolithFlagEntityData(const OctolithFlagEntityData&) noexcept = default;
        OctolithFlagEntityData& operator=(const OctolithFlagEntityData& other) noexcept;
    };
#pragma pack(pop)

    // size: 108
    struct FlagBaseEntityData
    {
        const EntityDataHeader Header{};
        const std::uint32_t TeamId = 0;
        const RawCollisionVolume Volume{};

        constexpr FlagBaseEntityData() noexcept = default;
        FlagBaseEntityData(const FlagBaseEntityData&) noexcept = default;
        FlagBaseEntityData& operator=(const FlagBaseEntityData& other) noexcept;
    };

    // size: 92
    struct TeleporterEntityData
    {
        const EntityDataHeader Header{};
        const std::uint8_t LoadIndex = 0;
        const std::uint8_t TargetIndex = 0;
        const std::uint8_t ArtifactId = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t Invisible = 0;
        char EntityFilename[15]{};
        const std::uint16_t Unused38 = 0;
        const std::uint16_t Unused3A = 0;
        const Vector3Fx TargetPosition{};
        char NodeName[16]{};

        constexpr TeleporterEntityData() noexcept = default;
        TeleporterEntityData(EntityDataHeader header, std::uint8_t loadIndex,
            std::uint8_t targetIndex, std::uint8_t artifactId, std::uint8_t active,
            std::uint8_t invisible, std::optional<std::string_view> entityFilename,
            Vector3Fx targetPosition, std::optional<std::string_view> nodeName) noexcept;
        TeleporterEntityData(const TeleporterEntityData&) noexcept = default;
        TeleporterEntityData& operator=(const TeleporterEntityData& other) noexcept;
    };

    // size: 104
    struct NodeDefenseEntityData
    {
        const EntityDataHeader Header{};
        const RawCollisionVolume Volume{};

        constexpr NodeDefenseEntityData() noexcept = default;
        NodeDefenseEntityData(const NodeDefenseEntityData&) noexcept = default;
        NodeDefenseEntityData& operator=(const NodeDefenseEntityData& other) noexcept;
    };

    // size: 136
    struct LightSourceEntityData
    {
        const EntityDataHeader Header{};
        const RawCollisionVolume Volume{};
        const std::uint8_t Light1Enabled = 0;
        const ColorRgb Light1Color{};
        const Vector3Fx Light1Vector{};
        const std::uint8_t Light2Enabled = 0;
        const ColorRgb Light2Color{};
        const Vector3Fx Light2Vector{};

        constexpr LightSourceEntityData() noexcept = default;
        LightSourceEntityData(const LightSourceEntityData&) noexcept = default;
        LightSourceEntityData& operator=(const LightSourceEntityData& other) noexcept;
    };

#pragma pack(push, 2)
    // size: 70
    struct ArtifactEntityData
    {
        const EntityDataHeader Header{};
        const std::uint8_t ModelId = 0;
        const std::uint8_t ArtifactId = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t HasBase = 0;
        const std::int16_t Message1Target = 0;
        const std::uint16_t Padding2A = 0;
        const Message Message1{};
        const std::int16_t Message2Target = 0;
        const std::uint16_t Padding32 = 0;
        const Message Message2{};
        const std::int16_t Message3Target = 0;
        const std::uint16_t Padding3A = 0;
        const Message Message3{};
        const std::int16_t LinkedEntityId = 0;

        constexpr ArtifactEntityData() noexcept = default;
        ArtifactEntityData(EntityDataHeader header, std::uint8_t modelId,
            std::uint8_t artifactId, std::uint8_t active, std::uint8_t hasBase,
            std::int16_t message1Target, Message message1,
            std::int16_t message2Target, Message message2,
            std::int16_t message3Target, Message message3,
            std::int16_t linkedEntityId) noexcept;
        ArtifactEntityData(const ArtifactEntityData&) noexcept = default;
        ArtifactEntityData& operator=(const ArtifactEntityData& other) noexcept;
    };
#pragma pack(pop)

    // size: 64
    struct CameraSequenceEntityData
    {
        const EntityDataHeader Header{};
        const std::uint8_t SequenceId = 0;
        const std::uint8_t Handoff = 0;
        const std::uint8_t Loop = 0;
        const std::uint8_t BlockInput = 0;
        const std::uint8_t ForceAltForm = 0;
        const std::uint8_t ForceBipedForm = 0;
        const std::uint16_t DelayFrames = 0;
        const std::uint8_t PlayerId1 = 0;
        const std::uint8_t PlayerId2 = 0;
        const std::int16_t Entity1 = 0;
        const std::int16_t Entity2 = 0;
        const std::int16_t EndMessageTargetId = 0;
        const Message EndMessage{};
        const std::int32_t EndMessageParam = 0;

        constexpr CameraSequenceEntityData() noexcept = default;
        CameraSequenceEntityData(const CameraSequenceEntityData&) noexcept = default;
        CameraSequenceEntityData& operator=(const CameraSequenceEntityData& other) noexcept;
    };

#pragma pack(push, 1)
    // size: 53
    struct ForceFieldEntityData
    {
        const EntityDataHeader Header{};
        const std::uint32_t Type = 0;
        const Fixed Width{};
        const Fixed Height{};
        const std::uint8_t Active = 0;

        constexpr ForceFieldEntityData() noexcept = default;
        ForceFieldEntityData(const ForceFieldEntityData&) noexcept = default;
        ForceFieldEntityData& operator=(const ForceFieldEntityData& other) noexcept;
    };
#pragma pack(pop)

    static_assert(sizeof(Entities::PlatformFlags) == 4);
    static_assert(sizeof(Entities::ObjectFlags) == 1);
    static_assert(sizeof(Entities::ObjEffFlags) == 4);
    static_assert(sizeof(Entities::TriggerFlags) == 4);
    static_assert(sizeof(Entities::FhTriggerFlags) == 4);
    static_assert(sizeof(RawCollisionVolume) == 64);
    static_assert(sizeof(FhRawCollisionVolume) == 64);

    static_assert(sizeof(Vector3FxArray8) == 96);
    static_assert(sizeof(Vector3FxArray10) == 120);
    static_assert(sizeof(Vector4FxArray10) == 160);
    static_assert(sizeof(Vector3FxArray16) == 192);
    static_assert(sizeof(EntityLengthArray) == 32);

    static_assert(sizeof(EntityHeader) == 36);
    static_assert(offsetof(EntityHeader, Version) == 0);
    static_assert(offsetof(EntityHeader, Lengths) == 4);
    static_assert(sizeof(EntityEntry) == 24);
    static_assert(offsetof(EntityEntry, LayerMask) == 16);
    static_assert(offsetof(EntityEntry, Length) == 18);
    static_assert(offsetof(EntityEntry, DataOffset) == 20);
    static_assert(sizeof(FhEntityEntry) == 20);
    static_assert(offsetof(FhEntityEntry, DataOffset) == 16);
    static_assert(sizeof(EntityDataHeader) == 40);
    static_assert(offsetof(EntityDataHeader, Type) == 0);
    static_assert(offsetof(EntityDataHeader, EntityId) == 2);
    static_assert(offsetof(EntityDataHeader, Position) == 4);
    static_assert(offsetof(EntityDataHeader, UpVector) == 16);
    static_assert(offsetof(EntityDataHeader, FacingVector) == 28);

    static_assert(sizeof(PlatformEntityData) == 588);
    static_assert(offsetof(PlatformEntityData, Positions) == 64);
    static_assert(offsetof(PlatformEntityData, Rotations) == 184);
    static_assert(offsetof(PlatformEntityData, PortalName) == 364);
    static_assert(offsetof(PlatformEntityData, LifetimeMsg4Param2) == 584);
    static_assert(sizeof(FhPlatformEntityData) == 236);
    static_assert(offsetof(FhPlatformEntityData, Volume) == 56);
    static_assert(offsetof(FhPlatformEntityData, Positions) == 120);
    static_assert(offsetof(FhPlatformEntityData, PortalName) == 220);
    static_assert(sizeof(ObjectEntityData) == 152);
    static_assert(offsetof(ObjectEntityData, Volume) == 88);

    static_assert(sizeof(PlayerSpawnEntityData) == 43);
    static_assert(alignof(PlayerSpawnEntityData) == 1);
    static_assert(offsetof(PlayerSpawnEntityData, TeamIndex) == 42);

    static_assert(sizeof(DoorEntityData) == 104);
    static_assert(offsetof(DoorEntityData, NodeName) == 40);
    static_assert(offsetof(DoorEntityData, EntityFilename) == 72);
    static_assert(offsetof(DoorEntityData, RoomName) == 88);
    static_assert(sizeof(FhDoorEntityData) == 64);
    static_assert(offsetof(FhDoorEntityData, RoomName) == 40);
    static_assert(sizeof(ItemSpawnEntityData) == 72);
    static_assert(offsetof(ItemSpawnEntityData, CollectedMessage) == 60);

    static_assert(sizeof(FhItemSpawnEntityData) == 50);
    static_assert(alignof(FhItemSpawnEntityData) == 1);
    static_assert(offsetof(FhItemSpawnEntityData, Unused2C) == 48);

    static_assert(sizeof(TriggerVolumeEntityData) == 160);
    static_assert(offsetof(TriggerVolumeEntityData, Volume) == 44);
    static_assert(offsetof(TriggerVolumeEntityData, TriggerFlags) == 120);
    static_assert(offsetof(TriggerVolumeEntityData, ChildMessage) == 148);
    static_assert(sizeof(FhTriggerVolumeEntityData) == 272);
    static_assert(offsetof(FhTriggerVolumeEntityData, Box) == 44);
    static_assert(offsetof(FhTriggerVolumeEntityData, Sphere) == 108);
    static_assert(offsetof(FhTriggerVolumeEntityData, Cylinder) == 172);
    static_assert(offsetof(FhTriggerVolumeEntityData, ChildMsgParam1) == 268);

    static_assert(sizeof(AreaVolumeEntityData) == 152);
    static_assert(offsetof(AreaVolumeEntityData, Volume) == 40);
    static_assert(offsetof(AreaVolumeEntityData, TriggerFlags) == 148);
    static_assert(sizeof(FhAreaVolumeEntityData) == 260);
    static_assert(offsetof(FhAreaVolumeEntityData, Box) == 44);
    static_assert(offsetof(FhAreaVolumeEntityData, TriggerFlags) == 256);

    static_assert(sizeof(JumpPadEntityData) == 148);
    static_assert(offsetof(JumpPadEntityData, Volume) == 48);
    static_assert(offsetof(JumpPadEntityData, TriggerFlags) == 144);
    static_assert(sizeof(FhJumpPadEntityData) == 272);
    static_assert(offsetof(FhJumpPadEntityData, Box) == 44);
    static_assert(offsetof(FhJumpPadEntityData, TriggerFlags) == 268);

    static_assert(sizeof(PointModuleEntityData) == 45);
    static_assert(alignof(PointModuleEntityData) == 1);
    static_assert(offsetof(PointModuleEntityData, Active) == 44);
    static_assert(sizeof(MorphCameraEntityData) == 104);
    static_assert(sizeof(FhMorphCameraEntityData) == 104);
    static_assert(sizeof(OctolithFlagEntityData) == 41);
    static_assert(alignof(OctolithFlagEntityData) == 1);
    static_assert(offsetof(OctolithFlagEntityData, TeamId) == 40);
    static_assert(sizeof(FlagBaseEntityData) == 108);
    static_assert(offsetof(FlagBaseEntityData, Volume) == 44);

    static_assert(sizeof(TeleporterEntityData) == 92);
    static_assert(offsetof(TeleporterEntityData, EntityFilename) == 45);
    static_assert(offsetof(TeleporterEntityData, Unused38) == 60);
    static_assert(offsetof(TeleporterEntityData, Unused3A) == 62);
    static_assert(offsetof(TeleporterEntityData, TargetPosition) == 64);
    static_assert(offsetof(TeleporterEntityData, NodeName) == 76);

    static_assert(sizeof(NodeDefenseEntityData) == 104);
    static_assert(sizeof(LightSourceEntityData) == 136);
    static_assert(offsetof(LightSourceEntityData, Light1Enabled) == 104);
    static_assert(offsetof(LightSourceEntityData, Light1Vector) == 108);
    static_assert(offsetof(LightSourceEntityData, Light2Enabled) == 120);
    static_assert(offsetof(LightSourceEntityData, Light2Vector) == 124);

    static_assert(sizeof(ArtifactEntityData) == 70);
    static_assert(alignof(ArtifactEntityData) == 2);
    static_assert(offsetof(ArtifactEntityData, Message1) == 48);
    static_assert(offsetof(ArtifactEntityData, Message2) == 56);
    static_assert(offsetof(ArtifactEntityData, Message3) == 64);
    static_assert(offsetof(ArtifactEntityData, LinkedEntityId) == 68);

    static_assert(sizeof(CameraSequenceEntityData) == 64);
    static_assert(offsetof(CameraSequenceEntityData, EndMessage) == 56);
    static_assert(offsetof(CameraSequenceEntityData, EndMessageParam) == 60);

    static_assert(sizeof(ForceFieldEntityData) == 53);
    static_assert(alignof(ForceFieldEntityData) == 1);
    static_assert(offsetof(ForceFieldEntityData, Active) == 52);
}
