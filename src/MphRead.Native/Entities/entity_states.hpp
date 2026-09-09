#pragma once

// Native counterpart of the per-entity runtime state in Entities/.
//
// Each managed entity class is mostly behaviour; these records are the data
// those classes carry, so the native implementations have one place to keep
// it and the two trees stay checkable against each other.  A C# reference
// member becomes a pointer to a forward-declared engine object; members
// marked `property` or `computed` are auto- or expression properties in the
// managed class and are storage here.

#include "Formats/Types.hpp"
#include "Formats/enum_tables.hpp"
#include "Entities/entity_records.hpp"
#include "Entities/entity_runtime_records.hpp"
#include "Formats/formats_layouts.hpp"
#include "Metadata/metadata.hpp"
#include "Metadata/weapon_metadata.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::entities {

using namespace fruityprime::formats;

// Engine objects these records reference, implemented elsewhere.
class EnemyInstanceEntity;

class Align;
class AreaVolumeEntityData;
class CameraInfo;
class CameraType;
class CollisionInstance;
class ConfirmState;
class DialogType;
class DoorEntity;
class DoorEntityData;
class EffectEntry;
class Enemy19Values;
class Enemy28Entity;
class Enemy29Entity;
class Enemy49Entity;
class EnemyBehavior<EnemyInstanceEntity>;
class EntityBase;
class EntityCollision;
class EquipInfo;
class FlagBaseEntityData;
class ForceFieldEntityData;
class HudObjectInstance;
class ItemSpawnEntity;
class Keybind;
class KeyboardState;
class Keys;
class ModelInstance;
class MouseButton;
class MouseState;
class Node;
class NodeData;
class NodeRef;
class ObjectEntityData;
class OctolithFlagEntityData;
class PlatformEntityData;
class PlayerControls;
class PlayerEntity;
class Portal;
class PromptType;
class RoomMetadata;
class SegmentInfo;
class SoundSource;
class TeleporterEntityData;
class TriggerVolumeEntityData;

// PlayerInput.cs
struct PlayerInputState {
    float MouseDeltaX{};
    float MouseDeltaY{};
    PlayerControls* Controls{};  // property
    KeyboardState* PrevKeyboardState{};  // property
    KeyboardState* KeyboardState{};  // property
    MouseState* PrevMouseState{};  // property
    MouseState* MouseState{};  // property
    float ClickX{};  // property
    float ClickY{};  // property
    bool HasInput{};  // property
    ButtonType Type{};  // property
    Keys* Key{};  // property
    MouseButton* MouseButton{};  // property
    bool IsPressed{};  // property
    bool IsDown{};  // property
    bool IsReleased{};  // property
    bool NeedsRepress{};  // property
    bool MouseAim{};  // property
    bool KeyboardAim{};  // property
    Keybind* MoveLeft{};  // property
    Keybind* MoveRight{};  // property
    Keybind* MoveUp{};  // property
    Keybind* MoveDown{};  // property
    Keybind* RolltLeft{};  // property
    Keybind* RollRight{};  // property
    Keybind* RollUp{};  // property
    Keybind* RollDown{};  // property
    Keybind* AimLeft{};  // property
    Keybind* AimRight{};  // property
    Keybind* AimUp{};  // property
    Keybind* AimDown{};  // property
    Keybind* Shoot{};  // property
    Keybind* Zoom{};  // property
    Keybind* Jump{};  // property
    Keybind* Morph{};  // property
    Keybind* Boost{};  // property
    Keybind* AltAttack{};  // property
    Keybind* ScanVisor{};  // property
    Keybind* Scan{};  // property
    Keybind* NextWeapon{};  // property
    Keybind* PrevWeapon{};  // property
    Keybind* WeaponMenu{};  // property
    Keybind* PowerBeam{};  // property
    Keybind* Missile{};  // property
    Keybind* VoltDriver{};  // property
    Keybind* Battlehammer{};  // property
    Keybind* Imperialist{};  // property
    Keybind* Judicator{};  // property
    Keybind* Magmaul{};  // property
    Keybind* ShockCoil{};  // property
    Keybind* OmegaCannon{};  // property
    Keybind* AffinitySlot{};  // property
    Keybind* Pause{};  // property
    Keybind* HudOverlay{};  // property
    bool InvertAimY{};  // property
    bool InvertAimX{};  // property
    bool ScrollAllWeapons{};  // property
    std::vector<Keybind*> All;  // property
};

// PlayerHud.cs
struct PlayerHudState {
    formats::Vector3 Position{};
    ModelInstance* Model{};
    formats::ColorRgb Color{};
    float Alpha{};
    std::int32_t _nodesHudState{};
    std::int32_t _nodesProgressAmount{};
    bool HudReady{};  // property
    bool ScanVisor{};  // property
    std::uint8_t HudDisruptedState{};  // property
    float HudDisruptionFactor{};  // property
    std::int32_t HudWhiteoutState{};  // property
    float HudWhiteoutFactor{};  // property
    float FontSize{};  // property
    float Lifetime{};  // property
    std::uint8_t Category{};  // property
    std::int32_t MaxWidth{};  // property
    Align* Align{};  // property
    std::vector<char> Text;  // property
    bool DialogHide{};  // property
};

// BeamProjectileEntity.cs
struct BeamProjectileState {
    BeamFlags Flags{};  // property
    BeamType Beam{};  // property
    BeamType BeamKind{};  // property
    formats::Vector3 Velocity{};  // property
    formats::Vector3 Acceleration{};  // property
    formats::Vector3 BackPosition{};  // property
    formats::Vector3 SpawnPosition{};  // property
    std::vector<formats::Vector3> PastPositions;  // property
    std::int32_t DrawFuncId{};  // property
    float Age{};  // property
    float Lifespan{};  // property
    formats::Vector3 Color{};  // property
    std::uint8_t CollisionEffect{};  // property
    std::uint8_t DamageDirType{};  // property
    std::uint8_t SplashDamageType{};  // property
    float Homing{};  // property
    formats::Vector3 Direction{};  // property
    formats::Vector3 Right{};  // property
    formats::Vector3 Up{};  // property
    float Damage{};  // property
    float HeadshotDamage{};  // property
    float SplashDamage{};  // property
    float SplashRadius{};  // property
    float MaxDistance{};  // property
    Affliction Afflictions{};  // property
    EntityBase* Owner{};  // property
    metadata::weapon_table::WeaponInfo RicochetWeapon{};  // property
    EffectEntry* Effect{};  // property
    EffectEntry* MuzzleEffect{};  // property
    EntityBase* Target{};  // property
    EquipInfo* Equip{};  // property
    std::int32_t DamageInterpolation{};  // property
    std::int32_t SpeedInterpolation{};  // property
    float SpeedDecayTime{};  // property
    float Speed{};  // property
    float InitialSpeed{};  // property
    float FinalSpeed{};  // property
    float DamageDirMag{};  // property
    float RicochetLossH{};  // property
    float RicochetLossV{};  // property
    float CylinderRadius{};  // property
};

// DoorEntity.cs
struct DoorState {
    formats::Vector3 LockPosition{};
    DoorEntityData* Data{};
    float Radius{};  // property
    float RadiusSquared{};  // property
    DoorFlags Flags{};  // property
    std::int32_t TargetRoomId{};  // property
    std::int32_t TargetLayerId{};  // property
    DoorEntity* LoaderDoor{};  // property
    DoorEntity* ConnectorDoor{};  // property
    Portal* Portal{};  // property
    ModelInstance* ConnectorModel{};  // property
    CollisionInstance* ConnectorCollision{};  // property
    bool ConnectorInactive{};  // property
};

// RoomEntity.cs
struct RoomState {
    std::vector<CollisionInstance*> RoomCollision;
    NodeData* NodeData{};
    RoomMetadata* Meta{};
    Portal* Portal{};
    std::int32_t NodeIndex{};
    std::int32_t RoomId{};  // property
    DoorEntity* LoaderDoor{};  // property
    std::int32_t LoadEntityId{};  // property
};

// PlatformEntity.cs
struct PlatformState {
    PlatStateFlags StateFlags{};
    PlatformEntityData* Data{};
    formats::Vector3 Velocity{};
    std::int32_t Id{};
    PlatSfxFlags Flags{};
    SfxData Start1{};
    SfxData Start2{};
    SfxData Stop{};
    SfxData Destoryed{};
    float Offset{};
    std::int32_t RangeIndex{};
};

// EnemyInstanceEntity.cs
struct EnemyInstanceState {
    EnemyType Type{};
    EntityBase* Spawner{};
    std::uint16_t Health{};
    std::uint16_t HealthMax{};
    std::uint8_t StateA{};
    std::uint8_t StateB{};
    std::vector<metadata::Effectiveness> BeamEffectiveness;
    CollisionVolume HurtVolume{};
    EnemyType EnemyType{};
    EntityBase* Owner{};
    std::uint8_t NextState{};
    std::vector<bool> HitPlayers;  // property
    EnemyFlags Flags{};  // property
    std::int32_t HealthbarMessageId{};  // property
    std::vector<EnemyBehavior<EnemyInstanceEntity>> Behaviors;  // property
};

// ItemInstanceEntity.cs
struct ItemInstanceState {
    formats::Vector3 Position{};
    ItemType ItemType{};
    std::int32_t DespawnTimer{};
    std::int32_t ParentId{};  // property
    ItemSpawnEntity* Owner{};  // property
    NodeData3 ClosestNode{};  // property
};

// 19_Cretaphid.cs
struct CretaphidState {
    Enemy19Values* Values{};
    SoundSource* SoundSource{};
    std::int32_t PhaseIndex{};  // property
    std::vector<EquipInfo*> EquipInfo;  // property
    std::vector<SegmentInfo*> Segments;  // property
    ModelInstance* BeamModel{};  // property
    ModelInstance* BeamColModel{};  // property
    float Angle{};  // property
    float AngleStep{};  // property
    float BeamAngle{};  // property
    float BeamAngleMax{};  // property
    float BeamAngleMin{};  // property
    float BeamAngleStep{};  // property
    Node* JointNode{};  // property
    std::uint16_t Unused1C{};  // property
    std::int8_t SpinDirection{};  // property
    bool InvertBeamRotation{};  // property
    std::uint16_t CrystalHealth{};  // property
    std::uint16_t PhaseFlashTime{};  // property
    std::uint16_t Phase0CrystalHealth{};  // property
    std::uint16_t Phase1CrystalHealth{};  // property
    std::uint16_t Phase2CrystalHealth{};  // property
    std::uint16_t Phase0CrystalShotTime{};  // property
    std::uint16_t Phase1CrystalShotTime{};  // property
    std::uint16_t Phase2CrystalShotTime{};  // property
    std::uint16_t Phase0CrystalShotDelay{};  // property
    std::uint16_t Phase1CrystalShotDelay{};  // property
    std::uint16_t Phase2CrystalShotDelay{};  // property
    std::uint16_t Phase0CrystalUpTime{};  // property
    std::uint16_t Phase1CrystalUpTime{};  // property
    std::uint16_t Phase2CrystalUpTime{};  // property
    std::vector<std::uint16_t> CrystalBeamDamage;  // property
    std::vector<std::uint16_t> EyeBeamDamage;  // property
    std::vector<std::uint16_t> EyeSplashDamage;  // property
    std::vector<std::uint16_t> EyeContactDamage;  // property
    std::int32_t Unused34{};  // property
    std::int32_t Unused38{};  // property
    std::int32_t Unused3C{};  // property
    std::int32_t Seg0AngleStep{};  // property
    std::int32_t Seg1AngleStep{};  // property
    std::int32_t Seg2AngleStep{};  // property
    std::int32_t Seg0BeamStartAngle{};  // property
    std::int32_t Seg1BeamStartAngle{};  // property
    std::int32_t Seg2BeamStartAngle{};  // property
    std::int32_t Seg0BeamAngleMin{};  // property
    std::int32_t Seg1BeamAngleMin{};  // property
    std::int32_t Seg2BeamAngleMin{};  // property
    std::int32_t Seg0BeamAngleMax{};  // property
    std::int32_t Seg1BeamAngleMax{};  // property
    std::int32_t Seg2BeamAngleMax{};  // property
    std::int32_t Seg0BeamAngleStep{};  // property
    std::int32_t Seg1BeamAngleStep{};  // property
    std::int32_t Seg2BeamAngleStep{};  // property
    std::uint16_t EyeHealth{};  // property
    std::uint8_t ItemChanceHealth{};  // property
    std::uint8_t ItemChanceMissile{};  // property
    std::uint8_t ItemChanceUa{};  // property
    std::uint8_t ItemChanceNone{};  // property
    std::vector<std::uint8_t> Phase0EyeState;  // property
    std::vector<std::uint8_t> Phase0BeamType;  // property
    std::vector<std::uint8_t> Phase0BeamSpawnMin;  // property
    std::vector<std::uint8_t> Phase0BeamSpawnMax;  // property
    std::vector<std::uint16_t> Phase0BeamCooldown;  // property
    std::vector<std::uint16_t> Phase0EyeStateTimer0;  // property
    std::vector<std::uint16_t> Phase0EyeStateTimer1;  // property
    std::vector<std::uint16_t> Phase0EyeStateTimer2;  // property
    std::vector<std::uint16_t> Phase0EyeStateTimer3;  // property
    std::vector<std::uint8_t> Phase1EyeState;  // property
    std::vector<std::uint8_t> Phase1BeamType;  // property
    std::vector<std::uint8_t> Phase1BeamSpawnMin;  // property
    std::vector<std::uint8_t> Phase1BeamSpawnMax;  // property
    std::vector<std::uint16_t> Phase1BeamCooldown;  // property
    std::vector<std::uint16_t> Phase1EyeStateTimer0;  // property
    std::vector<std::uint16_t> Phase1EyeStateTimer1;  // property
    std::vector<std::uint16_t> Phase1EyeStateTimer2;  // property
    std::vector<std::uint16_t> Phase1EyeStateTimer3;  // property
    std::vector<std::uint8_t> Phase2EyeState;  // property
    std::vector<std::uint8_t> Phase2BeamType;  // property
    std::vector<std::uint8_t> Phase2BeamSpawnMin;  // property
    std::vector<std::uint8_t> Phase2BeamSpawnMax;  // property
    std::vector<std::uint16_t> Phase2BeamCooldown;  // property
    std::vector<std::uint16_t> Phase2EyeStateTimer0;  // property
    std::vector<std::uint16_t> Phase2EyeStateTimer1;  // property
    std::vector<std::uint16_t> Phase2EyeStateTimer2;  // property
    std::vector<std::uint16_t> Phase2EyeStateTimer3;  // property
    std::uint8_t ItemChanceA{};  // property
    std::uint8_t ItemChanceB{};  // property
    std::uint8_t ItemChanceC{};  // property
    std::uint8_t ItemChanceD{};  // property
    std::uint16_t Padding27E{};  // property
    std::int32_t CollisionRadius{};  // property
    std::uint16_t ScanId{};  // property
    std::uint16_t CrystalScanId{};  // property
    std::uint32_t CrystalEffectiveness{};  // property
    std::int32_t EyeScanId{};  // property
    std::uint32_t EyeEffectiveness{};  // property
};

// ObjectEntity.cs
struct ObjectState {
    bool _effectActive{};
    ObjectEntityData* Data{};
    std::int32_t SfxId{};
    bool Environment{};
};

// ForceFieldEntity.cs
struct ForceFieldState {
    ForceFieldEntityData* Data{};
    formats::Vector3 FieldUpVector{};
    formats::Vector3 FieldFacingVector{};
    formats::Vector3 FieldRightVector{};
    formats::Vector4 Plane{};
    float Width{};
    float Height{};
    Enemy49Entity* Lock{};
};

// TeleporterEntity.cs
struct TeleporterState {
    TeleporterEntityData* Data{};
};

// EntityBase.cs
struct EntityBaseState {
    formats::Matrix4 CollisionTransform{};
    std::int32_t Id{};  // property
    EntityType Type{};  // property
    bool ShouldDraw{};  // property
    bool Initialized{};  // property
    bool Active{};  // property
    bool Hidden{};  // property
    float Alpha{};  // property
    NodeRef* NodeRef{};  // property
    std::vector<EntityCollision*> EntityCollision;  // property
};

// 20_CretaphidEye.cs
struct CretaphidEyeState {
    std::uint16_t BeamType{};  // property
    bool EyeActive{};  // property
    std::uint16_t BeamSpawnCount{};  // property
    std::int32_t BeamSpawnCooldown{};  // property
    std::int32_t BeamSpawnTimer{};  // property
    bool SpawnBurn{};  // property
    std::int32_t EyeIndex{};  // property
    bool BeamColliding{};  // property
    std::int32_t SegmentIndex{};  // property
};

// 26_GoreaArm.cs
struct GoreaArmState {
    EquipInfo* EquipInfo{};
    std::int32_t ColorTimer{};
    std::int32_t Index{};  // property
    GoreaArmFlags ArmFlags{};  // property
    std::int32_t Ammo{};  // property
    std::int32_t Damage{};  // property
    std::int32_t Cooldown{};  // property
    std::int32_t RegenTimer{};  // property
};

// 28_Gorea1B.cs
struct Gorea1BState {
    Enemy29Entity* SealSphere{};
    std::int32_t PhasesLeft{};
    bool _grappling{};
    float _field21C{};
    float _field21E{};
};

// 30_Trocra.cs
struct TrocraState {
    Enemy28Entity* Gorea1B{};  // property
    std::int32_t Index{};  // property
    formats::Vector3 Field174{};  // property
    std::int32_t Field184{};  // property
    std::int32_t State{};  // property
};

// 31_Gorea2.cs
struct Gorea2State {
    std::uint8_t Field244{};
    Gorea2Flags GoreaFlags{};  // property
};

// 41_Slench.cs
struct SlenchState {
    std::int32_t Subtype{};
    std::int32_t Phase{};
    float ShieldOffset{};
    SlenchFlags SlenchFlags{};  // property
    std::int32_t SynapseIndex{};  // property
    std::uint16_t ScanId1{};  // property
    std::uint16_t ScanId2{};  // property
    std::int32_t AngleIncrement1{};  // property
    std::int32_t Health{};  // property
    std::int32_t AngleIncrement2{};  // property
    std::uint16_t MinStaticShotTimer{};  // property
    std::uint16_t MaxStaticShotTimer{};  // property
    std::int16_t StaticShotCooldown{};  // property
    std::uint8_t StaticShotCount{};  // property
    std::uint8_t Padding17{};  // property
    std::int32_t AngleIncrement3{};  // property
    std::int32_t MoveIncrement1{};  // property
    std::int32_t MoveIncrement2{};  // property
    std::int32_t AngleIncrement4{};  // property
    std::int32_t RoamTime{};  // property
    std::int32_t MoveIncrement3{};  // property
    std::int32_t RollTime{};  // property
    std::int32_t FloatingAngleInc{};  // property
    std::int32_t RollingAngleInc{};  // property
    std::int32_t FloatingSpeed{};  // property
    std::int32_t RollingSpeed{};  // property
    std::int32_t AngleIncrement5{};  // property
    std::int32_t SlamRange{};  // property
    std::int32_t MoveIncrement4{};  // property
    std::int32_t MoveIncrement5{};  // property
    std::uint16_t SlamDelay{};  // property
    std::uint8_t WobbleCycles{};  // property
    std::uint8_t WobbleRotInc{};  // property
    std::int32_t MaxWobbleDist{};  // property
    std::uint16_t Magic{};  // property
    std::uint16_t Padding5E{};  // property
};

// PlayerPause.cs
struct PlayerPauseState {
    bool Unlocked{};  // property
    std::int32_t Group{};  // property
    std::int32_t MessageId{};  // property
    std::int32_t OffsetX{};  // property
    std::int32_t OffsetY{};  // property
    HudObjectInstance* HudObject{};  // property
    std::int32_t ObjectIndex{};  // property
};

// PlayerScan.cs
struct PlayerScanState {
    EntityBase* Entity{};  // property
    float Distance{};  // property
    float CenterDist{};  // property
    std::int32_t Category{};  // property
    formats::Vector3 Position{};  // property
    float ScreenX{};  // property
    float ScreenY{};  // property
    float Scale{};  // property
    bool Dim{};  // property
};

// PlayerCamera.cs
struct PlayerCameraState {
    formats::Vector3 Position{};
    formats::Vector3 PrevPosition{};
    formats::Vector3 Target{};
    formats::Vector3 UpVector{};
    formats::Vector3 TrueUp{};
    formats::Vector3 Facing{};
    float Fov{};
    float Shake{};
    formats::Matrix4 ViewMatrix{};
    float Field48{};
    float Field4C{};
    float Field50{};
    float Field54{};
    NodeRef* NodeRef{};
    CameraInfo* CameraInfo{};  // property
    CameraType* CameraType{};  // property
};

// PlayerDialog.cs
struct PlayerDialogState {
    float Left{};
    float Right{};
    float Top{};
    float Bottom{};
    DialogType* DialogType{};  // property
    ConfirmState* DialogConfirmState{};  // property
    PromptType* DialogPromptType{};  // property
};

// HalfturretEntity.cs
struct HalfturretState {
    EntityBase* Target{};
    PlayerEntity* Owner{};  // property
    NodeData3 ClosestNode{};  // property
    EquipInfo* EquipInfo{};  // property
};

// JumpPadEntity.cs
struct JumpPadState {
    NodeData3 ClosestNode{};  // property
};

// ArtifactEntity.cs
struct ArtifactState {
    std::uint8_t ModelId{};
    std::uint8_t ArtifactId{};
};

// OctolithFlagEntity.cs
struct OctolithFlagState {
    OctolithFlagEntityData* Data{};
    formats::Vector3 BasePosition{};
    PlayerEntity* Carrier{};
    bool AtBase{};
    NodeData3 ClosestNode{};  // property
    NodeData3 BaseClosestNode{};  // property
};

// FlagBaseEntity.cs
struct FlagBaseState {
    FlagBaseEntityData* Data{};
    NodeData3 ClosestNode{};  // property
};

// NodeDefenseEntity.cs
struct NodeDefenseState {
    CollisionVolume Volume{};
    PlayerEntity* CapturedPlayer{};
    bool Contested{};
    bool InProgress{};
    std::int32_t CurrentTeam{};
    std::int32_t OccupyingTeam{};
    bool Blinking{};
    std::vector<bool> OccupiedBy;
    bool IsOccupied{};
    float Progress{};
    NodeData3 ClosestNode{};  // property
};

// LightSourceEntity.cs
struct LightSourceState {
    CollisionVolume Volume{};  // property
    bool Light1Enabled{};  // property
    formats::Vector3 Light1Vector{};  // property
    formats::Vector3 Light1Color{};  // property
    bool Light2Enabled{};  // property
    formats::Vector3 Light2Vector{};  // property
    formats::Vector3 Light2Color{};  // property
};

// TriggerVolumeEntity.cs
struct TriggerVolumeState {
    CollisionVolume Volume{};
    TriggerVolumeEntityData* Data{};
};

// AreaVolumeEntity.cs
struct AreaVolumeState {
    AreaVolumeEntityData* Data{};
};

// BombEntity.cs
struct BombState {
    BombFlags Flags{};  // property
    PlayerEntity* Owner{};  // property
    BombType BombType{};  // property
    std::int32_t BombIndex{};  // property
    std::int32_t Countdown{};  // property
    float Radius{};  // property
    float SelfRadius{};  // property
    std::uint16_t Damage{};  // property
    std::uint16_t EnemyDamage{};  // property
    EffectEntry* Effect{};  // property
};

} // namespace fruityprime::entities
