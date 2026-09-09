#pragma once

// Native counterpart of the editor records in Formats/EntityClass.cs: the
// mutable view of each entity payload that the editor and the repacker write
// back.  Transliterated from the managed source.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Formats/formats_layouts.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

// EntityClass.EntityEditorBase: the fields every editor record carries.
struct EntityEditorBase {
    EntityType Type{};
    std::int16_t Id{};
    std::uint16_t LayerMask{};
    Vector3 Position{};
    Vector3 Up{};
    Vector3 Facing{};
    std::string NodeName;
};

struct PlatformEntityEditor {
    std::uint32_t NoPort{};
    std::uint32_t ModelId{};
    std::int16_t ParentId{};
    bool Active{};
    std::uint8_t Delay{};
    std::uint16_t ScanData1{};
    std::int16_t ScanMsgTarget{};
    Message ScanMessage{};
    std::uint16_t ScanData2{};
    std::uint16_t PositionCount{};
    std::vector<formats::Vector3> Positions;
    std::vector<formats::Vector4> Rotations;
    formats::Vector3 PositionOffset{};
    float ForwardSpeed{};
    float BackwardSpeed{};
    std::string_view PortalName;
    std::uint32_t MovementType{};
    bool ForCutscene{};
    std::uint32_t ReverseType{};
    PlatformFlags Flags{};
    std::uint32_t ContactDamage{};
    formats::Vector3 BeamSpawnDir{};
    formats::Vector3 BeamSpawnPos{};
    std::int32_t BeamId{};
    std::uint32_t BeamInterval{};
    std::uint32_t BeamOnIntervals{};
    std::int32_t ResistEffectId{};
    std::uint32_t Health{};
    std::uint32_t Effectiveness{};
    std::int32_t DamageEffectId{};
    std::int32_t DeadEffectId{};
    std::uint8_t ItemChance{};
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

struct FhPlatformEntityEditor {
    std::uint32_t NoPortal{};
    std::uint32_t GroupId{};
    std::uint32_t Unused2C{};
    std::uint8_t Delay{};
    std::uint8_t PositionCount{};
    CollisionVolume Volume{};
    std::vector<formats::Vector3> Positions;
    float Speed{};
    std::string_view PortalName;
};

struct ObjectEntityEditor {
    ObjectFlags Flags{};
    ObjEffFlags EffectFlags{};
    std::int32_t ModelId{};
    std::int16_t LinkedEntity{};
    std::uint16_t ScanId{};
    std::int16_t ScanMsgTarget{};
    Message ScanMessage{};
    std::int32_t EffectId{};
    std::uint32_t EffectInterval{};
    std::uint32_t EffectOnIntervals{};
    formats::Vector3 EffectPositionOffset{};
    CollisionVolume Volume{};
};

struct PlayerSpawnEntityEditor {
    std::uint8_t Availability{};
    bool Active{};
    std::int8_t TeamIndex{};
};

struct DoorEntityEditor {
    std::string_view DoorNodeName;
    std::uint32_t PaletteId{};
    DoorType DoorType{};
    std::uint32_t ConnectorId{};
    std::uint8_t TargetLayerId{};
    bool Locked{};
    std::uint8_t Field42{};
    std::uint8_t Field43{};
    std::string_view EntityFilename;
    std::string_view RoomName;
};

struct FhDoorEntityEditor {
    std::string_view RoomName;
    bool Locked{};
    std::uint32_t ModelId{};
};

struct ItemSpawnEntityEditor {
    std::int32_t ParentId{};
    ItemType ItemType{};
    bool Enabled{};
    bool HasBase{};
    bool AlwaysActive{};
    std::uint16_t MaxSpawnCount{};
    std::uint16_t SpawnInterval{};
    std::uint16_t SpawnDelay{};
    std::int16_t NotifyEntityId{};
    Message CollectedMessage{};
    std::int32_t CollectedMsgParam1{};
    std::int32_t CollectedMsgParam2{};
};

struct FhItemSpawnEntityEditor {
    FhItemType ItemType{};
    std::uint16_t SpawnLimit{};
    std::uint16_t CooldownTime{};
    std::uint16_t Unused2C{};
};

struct TriggerVolumeEntityEditor {
    TriggerType Subtype{};
    CollisionVolume Volume{};
    bool Active{};
    bool AlwaysActive{};
    bool DeactivateAfterUse{};
    std::uint16_t RepeatDelay{};
    std::uint16_t CheckDelay{};
    std::uint16_t RequiredStateBit{};
    TriggerFlags TriggerFlags{};
    std::uint32_t TriggerThreshold{};
    std::int16_t ParentId{};
    Message ParentMessage{};
    std::int32_t ParentMsgParam1{};
    std::int32_t ParentMsgParam2{};
    std::int16_t ChildId{};
    Message ChildMessage{};
    std::int32_t ChildMsgParam1{};
    std::int32_t ChildMsgParam2{};
};

struct FhTriggerVolumeEntityEditor {
    FhTriggerType Subtype{};
    CollisionVolume Box{};
    CollisionVolume Sphere{};
    CollisionVolume Cylinder{};
    std::uint16_t OneUse{};
    std::uint16_t Cooldown{};
    FhTriggerFlags TriggerFlags{};
    std::uint32_t Threshold{};
    std::int16_t ParentId{};
    FhMessage ParentMessage{};
    std::int32_t ParentMsgParam1{};
    std::int16_t ChildId{};
    FhMessage ChildMessage{};
    std::int32_t ChildMsgParam1{};
};

struct AreaVolumeEntityEditor {
    CollisionVolume Volume{};
    bool Active{};
    bool AlwaysActive{};
    bool AllowMultiple{};
    std::uint8_t MessageDelay{};
    std::uint16_t Unused6A{};
    Message InsideMessage{};
    std::int32_t InsideMsgParam1{};
    std::int32_t InsideMsgParam2{};
    std::int16_t ParentId{};
    Message ExitMessage{};
    std::int32_t ExitMsgParam1{};
    std::int32_t ExitMsgParam2{};
    std::int16_t ChildId{};
    std::uint16_t Cooldown{};
    std::uint32_t Priority{};
    TriggerFlags TriggerFlags{};
};

struct FhAreaVolumeEntityEditor {
    FhTriggerType Subtype{};
    CollisionVolume Box{};
    CollisionVolume Sphere{};
    CollisionVolume Cylinder{};
    FhMessage InsideMessage{};
    std::int32_t InsideMsgParam1{};
    FhMessage ExitMessage{};
    std::int32_t ExitMsgParam1{};
    std::uint16_t Cooldown{};
    FhTriggerFlags TriggerFlags{};
};

struct JumpPadEntityEditor {
    std::int32_t ParentId{};
    std::uint32_t Unused28{};
    CollisionVolume Volume{};
    formats::Vector3 BeamVector{};
    float Speed{};
    std::uint16_t ControlLockTime{};
    std::uint16_t CooldownTime{};
    bool Active{};
    std::uint32_t ModelId{};
    std::uint32_t BeamType{};
    TriggerFlags TriggerFlags{};
};

struct FhJumpPadEntityEditor {
    FhTriggerType VolumeType{};
    CollisionVolume Box{};
    CollisionVolume Sphere{};
    CollisionVolume Cylinder{};
    std::uint32_t CooldownTime{};
    formats::Vector3 BeamVector{};
    float Speed{};
    std::uint32_t ControlLockTime{};
    std::uint32_t ModelId{};
    std::uint32_t BeamType{};
    FhTriggerFlags TriggerFlags{};
};

struct PointModuleEntityEditor {
    std::int16_t NextId{};
    std::int16_t PrevId{};
    bool Active{};
};

struct MorphCameraEntityEditor {
    CollisionVolume Volume{};
};

struct OctolithFlagEntityEditor {
    std::uint8_t TeamId{};
};

struct FlagBaseEntityEditor {
    std::uint32_t TeamId{};
    CollisionVolume Volume{};
};

struct TeleporterEntityEditor {
    std::uint8_t LoadIndex{};
    std::uint8_t TargetIndex{};
    std::uint8_t ArtifactId{};
    bool Active{};
    bool Invisible{};
    std::string_view TargetRoom;
    formats::Vector3 TargetPosition{};
    std::string_view TeleporterNodeName;
};

struct NodeDefenseEntityEditor {
    CollisionVolume Volume{};
};

struct LightSourceEntityEditor {
    CollisionVolume Volume{};
    bool Light1Enabled{};
    formats::ColorRgb Light1Color{};
    formats::Vector3 Light1Vector{};
    bool Light2Enabled{};
    formats::ColorRgb Light2Color{};
    formats::Vector3 Light2Vector{};
};

struct ArtifactEntityEditor {
    std::uint8_t ModelId{};
    std::uint8_t ArtifactId{};
    bool Active{};
    bool HasBase{};
    std::int16_t Message1Target{};
    Message Message1{};
    std::int16_t Message2Target{};
    Message Message2{};
    std::int16_t Message3Target{};
    Message Message3{};
    std::int16_t LinkedEntityId{};
};

struct CameraSequenceEntityEditor {
    std::uint8_t SequenceId{};
    bool Handoff{};
    bool Loop{};
    bool BlockInput{};
    bool ForceAltForm{};
    bool ForceBipedForm{};
    std::uint16_t DelayFrames{};
    std::uint8_t PlayerId1{};
    std::uint8_t PlayerId2{};
    std::int16_t Entity1{};
    std::int16_t Entity2{};
    std::int16_t EndMessageTargetId{};
    Message EndMessage{};
    std::int32_t EndMessageParam{};
};

struct ForceFieldEntityEditor {
    std::uint32_t ForceFieldType{};
    float Width{};
    float Height{};
    bool Active{};
};

} // namespace fruityprime::formats
