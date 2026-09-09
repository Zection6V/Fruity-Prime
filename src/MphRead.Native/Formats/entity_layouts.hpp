#pragma once

// Native counterpart of Formats/Entity.cs.
// The per-type entity payload layouts as stored in a room's entity file.
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

struct EntityHeader {
    std::uint32_t Version{};
    EntityLengthArray Lengths{};
};

struct EntityEntry {
    std::vector<char> NodeName;
    std::uint16_t LayerMask{};
    std::uint16_t Length{};
    std::uint32_t DataOffset{};
};

struct FhEntityEntry {
    std::vector<char> NodeName;
    std::uint32_t DataOffset{};
};

struct EntityDataHeader {
    std::uint16_t Type{};
    std::int16_t EntityId{};
    formats::Vector3Fx Position{};
    formats::Vector3Fx UpVector{};
    formats::Vector3Fx FacingVector{};
};

struct PlatformEntityData {
    EntityDataHeader Header{};
    std::uint32_t NoPort{};
    std::uint32_t ModelId{};
    std::int16_t ParentId{};
    std::uint8_t Active{};
    std::uint8_t Delay{};
    std::uint16_t ScanData1{};
    std::int16_t ScanMsgTarget{};
    Message ScanMessage{};
    std::uint16_t ScanData2{};
    std::uint16_t PositionCount{};
    Vector3FxArray10 Positions{};
    Vector4FxArray10 Rotations{};
    formats::Vector3Fx PositionOffset{};
    formats::Fixed ForwardSpeed{};
    formats::Fixed BackwardSpeed{};
    std::vector<char> PortalName;
    std::uint32_t MovementType{};
    std::uint32_t ForCutscene{};
    std::uint32_t ReverseType{};
    PlatformFlags Flags{};
    std::uint32_t ContactDamage{};
    formats::Vector3Fx BeamSpawnDir{};
    formats::Vector3Fx BeamSpawnPos{};
    std::int32_t BeamId{};
    std::uint32_t BeamInterval{};
    std::uint32_t BeamOnIntervals{};
    std::uint16_t Unused1B0{};
    std::uint16_t Unused1B2{};
    std::int32_t ResistEffectId{};
    std::uint32_t Health{};
    std::uint32_t Effectiveness{};
    std::int32_t DamageEffectId{};
    std::int32_t DeadEffectId{};
    std::uint8_t ItemChance{};
    std::uint8_t Padding1C9{};
    std::uint16_t Padding1CA{};
    ItemType ItemType{};
    std::uint32_t Unused1D0{};
    std::uint32_t Unused1D4{};
    std::int32_t BeamHitMsgTarget{};
    Message BeamHitMessage{};
    std::int32_t BeamHitMsgParam1{};
    std::int32_t BeamHitMsgParam2{};
    std::int32_t PlayerColMsgTarget{};
    Message PlayerColMessage{};
    std::int32_t PlayerColMsgParam1{};
    std::int32_t PlayerColMsgParam2{};
    std::int32_t DeadMsgTarget{};
    Message DeadMessage{};
    std::int32_t DeadMsgParam1{};
    std::int32_t DeadMsgParam2{};
    std::uint16_t LifetimeMsg1Index{};
    std::int16_t LifetimeMsg1Target{};
    Message LifetimeMessage1{};
    std::int32_t LifetimeMsg1Param1{};
    std::int32_t LifetimeMsg1Param2{};
    std::uint16_t LifetimeMsg2Index{};
    std::int16_t LifetimeMsg2Target{};
    Message LifetimeMessage2{};
    std::int32_t LifetimeMsg2Param1{};
    std::int32_t LifetimeMsg2Param2{};
    std::uint16_t LifetimeMsg3Index{};
    std::int16_t LifetimeMsg3Target{};
    Message LifetimeMessage3{};
    std::int32_t LifetimeMsg3Param1{};
    std::int32_t LifetimeMsg3Param2{};
    std::uint16_t LifetimeMsg4Index{};
    std::int16_t LifetimeMsg4Target{};
    Message LifetimeMessage4{};
    std::int32_t LifetimeMsg4Param1{};
    std::int32_t LifetimeMsg4Param2{};
};

struct FhPlatformEntityData {
    EntityDataHeader Header{};
    std::uint32_t NoPortal{};
    std::uint32_t GroupId{};
    std::uint32_t Unused2C{};
    std::uint8_t Delay{};
    std::uint8_t PositionCount{};
    std::uint16_t Padding32{};
    raw::FhRawCollisionVolume Volume{};
    Vector3FxArray8 Positions{};
    formats::Fixed Speed{};
    std::vector<char> PortalName;
};

struct ObjectEntityData {
    EntityDataHeader Header{};
    ObjectFlags Flags{};
    std::uint8_t Padding25{};
    std::uint16_t Padding26{};
    ObjEffFlags EffectFlags{};
    std::int32_t ModelId{};
    std::int16_t LinkedEntity{};
    std::uint16_t ScanId{};
    std::int16_t ScanMsgTarget{};
    std::uint16_t Padding36{};
    Message ScanMessage{};
    std::int32_t EffectId{};
    std::uint32_t EffectInterval{};
    std::uint32_t EffectOnIntervals{};
    formats::Vector3Fx EffectPositionOffset{};
    raw::RawCollisionVolume Volume{};
};

struct PlayerSpawnEntityData {
    EntityDataHeader Header{};
    std::uint8_t Availability{};
    std::uint8_t Active{};
    std::int8_t TeamIndex{};
};

struct DoorEntityData {
    EntityDataHeader Header{};
    std::vector<char> NodeName;
    std::uint32_t PaletteId{};
    DoorType DoorType{};
    std::uint32_t ConnectorId{};
    std::uint8_t TargetLayerId{};
    std::uint8_t Locked{};
    std::uint8_t OutConnectorId{};
    std::uint8_t OutLoaderId{};
    std::vector<char> EntityFilename;
    std::vector<char> RoomName;
};

struct FhDoorEntityData {
    EntityDataHeader Header{};
    std::vector<char> RoomName;
    std::uint32_t Locked{};
    std::uint32_t ModelId{};
};

struct ItemSpawnEntityData {
    EntityDataHeader Header{};
    std::int32_t ParentId{};
    ItemType ItemType{};
    std::uint8_t Enabled{};
    std::uint8_t HasBase{};
    std::uint8_t AlwaysActive{};
    std::uint8_t Padding2F{};
    std::uint16_t MaxSpawnCount{};
    std::uint16_t SpawnInterval{};
    std::uint16_t SpawnDelay{};
    std::int16_t NotifyEntityId{};
    Message CollectedMessage{};
    std::int32_t CollectedMsgParam1{};
    std::int32_t CollectedMsgParam2{};
};

struct FhItemSpawnEntityData {
    EntityDataHeader Header{};
    FhItemType ItemType{};
    std::uint16_t SpawnLimit{};
    std::uint16_t CooldownTime{};
    std::uint16_t Unused2C{};
};

struct TriggerVolumeEntityData {
    EntityDataHeader Header{};
    TriggerType Subtype{};
    raw::RawCollisionVolume Volume{};
    std::uint16_t Unused68{};
    std::uint8_t Active{};
    std::uint8_t AlwaysActive{};
    std::uint8_t DeactivateAfterUse{};
    std::uint8_t Padding6D{};
    std::uint16_t RepeatDelay{};
    std::uint16_t CheckDelay{};
    std::uint16_t RequiredStateBit{};
    TriggerFlags TriggerFlags{};
    std::uint32_t TriggerThreshold{};
    std::int16_t ParentId{};
    std::uint16_t Padding7E{};
    Message ParentMessage{};
    std::int32_t ParentMsgParam1{};
    std::int32_t ParentMsgParam2{};
    std::int16_t ChildId{};
    std::uint16_t Padding8E{};
    Message ChildMessage{};
    std::int32_t ChildMsgParam1{};
    std::int32_t ChildMsgParam2{};
};

struct FhTriggerVolumeEntityData {
    EntityDataHeader Header{};
    FhTriggerType Subtype{};
    raw::FhRawCollisionVolume Box{};
    raw::FhRawCollisionVolume Sphere{};
    raw::FhRawCollisionVolume Cylinder{};
    std::uint16_t OneUse{};
    std::uint16_t Cooldown{};
    FhTriggerFlags TriggerFlags{};
    std::uint32_t Threshold{};
    std::int16_t ParentId{};
    std::uint16_t PaddingF6{};
    FhMessage ParentMessage{};
    std::int32_t ParentMsgParam1{};
    std::int16_t ChildId{};
    std::uint16_t Padding102{};
    FhMessage ChildMessage{};
    std::int32_t ChildMsgParam1{};
};

struct AreaVolumeEntityData {
    EntityDataHeader Header{};
    raw::RawCollisionVolume Volume{};
    std::uint16_t Unused64{};
    std::uint8_t Active{};
    std::uint8_t AlwaysActive{};
    std::uint8_t AllowMultiple{};
    std::uint8_t MessageDelay{};
    std::uint16_t Unused6A{};
    Message InsideMessage{};
    std::int32_t InsideMsgParam1{};
    std::int32_t InsideMsgParam2{};
    std::int16_t ParentId{};
    std::uint16_t Padding7A{};
    Message ExitMessage{};
    std::int32_t ExitMsgParam1{};
    std::int32_t ExitMsgParam2{};
    std::int16_t ChildId{};
    std::uint16_t Cooldown{};
    std::uint32_t Priority{};
    TriggerFlags TriggerFlags{};
};

struct FhAreaVolumeEntityData {
    EntityDataHeader Header{};
    FhTriggerType Subtype{};
    raw::FhRawCollisionVolume Box{};
    raw::FhRawCollisionVolume Sphere{};
    raw::FhRawCollisionVolume Cylinder{};
    FhMessage InsideMessage{};
    std::int32_t InsideMsgParam1{};
    FhMessage ExitMessage{};
    std::int32_t ExitMsgParam1{};
    std::uint16_t Cooldown{};
    std::uint16_t PaddingFA{};
    FhTriggerFlags TriggerFlags{};
};

struct JumpPadEntityData {
    EntityDataHeader Header{};
    std::int32_t ParentId{};
    std::uint32_t Unused28{};
    raw::RawCollisionVolume Volume{};
    formats::Vector3Fx BeamVector{};
    formats::Fixed Speed{};
    std::uint16_t ControlLockTime{};
    std::uint16_t CooldownTime{};
    std::uint8_t Active{};
    std::uint8_t Padding81{};
    std::uint16_t Padding82{};
    std::uint32_t ModelId{};
    std::uint32_t BeamType{};
    TriggerFlags TriggerFlags{};
};

struct FhJumpPadEntityData {
    EntityDataHeader Header{};
    FhTriggerType VolumeType{};
    raw::FhRawCollisionVolume Box{};
    raw::FhRawCollisionVolume Sphere{};
    raw::FhRawCollisionVolume Cylinder{};
    std::uint32_t CooldownTime{};
    formats::Vector3Fx BeamVector{};
    formats::Fixed Speed{};
    std::uint32_t ControlLockTime{};
    std::uint32_t ModelId{};
    std::uint32_t BeamType{};
    FhTriggerFlags TriggerFlags{};
};

struct PointModuleEntityData {
    EntityDataHeader Header{};
    std::int16_t NextId{};
    std::int16_t PrevId{};
    std::uint8_t Active{};
};

struct MorphCameraEntityData {
    EntityDataHeader Header{};
    raw::RawCollisionVolume Volume{};
};

struct FhMorphCameraEntityData {
    EntityDataHeader Header{};
    raw::FhRawCollisionVolume Volume{};
};

struct OctolithFlagEntityData {
    EntityDataHeader Header{};
    std::uint8_t TeamId{};
};

struct FlagBaseEntityData {
    EntityDataHeader Header{};
    std::uint32_t TeamId{};
    raw::RawCollisionVolume Volume{};
};

struct TeleporterEntityData {
    EntityDataHeader Header{};
    std::uint8_t LoadIndex{};
    std::uint8_t TargetIndex{};
    std::uint8_t ArtifactId{};
    std::uint8_t Active{};
    std::uint8_t Invisible{};
    std::vector<char> EntityFilename;
    std::uint16_t Unused38{};
    std::uint16_t Unused3A{};
    formats::Vector3Fx TargetPosition{};
    std::vector<char> NodeName;
};

struct NodeDefenseEntityData {
    EntityDataHeader Header{};
    raw::RawCollisionVolume Volume{};
};

struct LightSourceEntityData {
    EntityDataHeader Header{};
    raw::RawCollisionVolume Volume{};
    std::uint8_t Light1Enabled{};
    formats::ColorRgb Light1Color{};
    formats::Vector3Fx Light1Vector{};
    std::uint8_t Light2Enabled{};
    formats::ColorRgb Light2Color{};
    formats::Vector3Fx Light2Vector{};
};

struct ArtifactEntityData {
    EntityDataHeader Header{};
    std::uint8_t ModelId{};
    std::uint8_t ArtifactId{};
    std::uint8_t Active{};
    std::uint8_t HasBase{};
    std::int16_t Message1Target{};
    std::uint16_t Padding2A{};
    Message Message1{};
    std::int16_t Message2Target{};
    std::uint16_t Padding32{};
    Message Message2{};
    std::int16_t Message3Target{};
    std::uint16_t Padding3A{};
    Message Message3{};
    std::int16_t LinkedEntityId{};
};

struct CameraSequenceEntityData {
    EntityDataHeader Header{};
    std::uint8_t SequenceId{};
    std::uint8_t Handoff{};
    std::uint8_t Loop{};
    std::uint8_t BlockInput{};
    std::uint8_t ForceAltForm{};
    std::uint8_t ForceBipedForm{};
    std::uint16_t DelayFrames{};
    std::uint8_t PlayerId1{};
    std::uint8_t PlayerId2{};
    std::int16_t Entity1{};
    std::int16_t Entity2{};
    std::int16_t EndMessageTargetId{};
    Message EndMessage{};
    std::int32_t EndMessageParam{};
};

struct ForceFieldEntityData {
    EntityDataHeader Header{};
    std::uint32_t Type{};
    formats::Fixed Width{};
    formats::Fixed Height{};
    std::uint8_t Active{};
};

} // namespace fruityprime::formats
