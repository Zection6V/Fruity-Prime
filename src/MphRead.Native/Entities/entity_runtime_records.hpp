#pragma once

// Native counterpart of the plain record types declared beside the entity
// implementations in Entities/.  Several are private nested classes in C#;
// they are the data those implementations carry, and the port needs them all
// the same.  A C# reference member becomes a pointer to a forward-declared
// engine object.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Entities/entity_records.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::entities {

using namespace fruityprime::formats;

// NodeData's node kind, declared with the node-data reader.
enum class NodeType : std::uint32_t;

class AiPersonalityData1;
class Align;
class DoorEntity;
class HalfturretEntity;
class HudObjectInstance;
class ItemInstanceEntity;
class ItemSpawnEntity;
class ModelInstance;
class NodeDefenseEntity;
class PlayerEntity;
class Portal;

struct AiButton;
struct AiButtons;
struct AiTouchButtons;
struct AiPlayerAggro;
struct AiContext;
struct LocatorInfo;
struct HudMessage;
struct MapLegendInfo;
struct SfxData;
struct MoveSfxInfo;
struct EffectSfxInfo;
struct PortalNodeRef;
struct NodeData3;
struct AiGlobals;
struct AiEntityRefs;

// PlayerAi.cs
struct AiButton {
    bool IsDown{};
    std::int32_t FramesDown{};
    std::int32_t FramesUp{};
};

// PlayerAi.cs
struct AiButtons {
    AiButton Up{};
    AiButton Down{};
    AiButton Left{};
    AiButton Right{};
    AiButton A{};
    AiButton B{};
    AiButton X{};
    AiButton Y{};
    AiButton L{};
    AiButton R{};
    AiButton Start{};
    AiButton Select{};
    std::vector<AiButton> AllButtons;
};

// PlayerAi.cs
struct AiTouchButtons {
    AiButton Morph{};
    AiButton Unmorph{};
    AiButton PowerBeam{};
    AiButton Missile{};
    AiButton VoltDriver{};
    AiButton Battlehammer{};
    AiButton Imperialist{};
    AiButton Judicator{};
    AiButton Magmaul{};
    AiButton ShockCoil{};
    AiButton OmegaCannon{};
    std::vector<AiButton> AllButtons;
};

// PlayerAi.cs
struct AiPlayerAggro {
    std::uint8_t VarA2{};
    std::uint8_t VarA9{};
    std::uint8_t VarA3{};
    std::uint8_t VarA4{};
    std::uint8_t VarA10{};
    std::uint16_t VarA7{};
    std::uint16_t Staleness{};
    std::uint16_t Expiration{};
    PlayerEntity* Player1{};
    PlayerEntity* Player2{};
};

// PlayerAi.cs
struct AiContext {
    std::int32_t Func24Id{};
    std::uint8_t Field4{};
    std::uint8_t Field5{};
    std::uint8_t Field6{};
    std::uint8_t Field7{};
    std::uint8_t Field8{};
    std::uint8_t Field9{};
    std::uint8_t FieldA{};
    std::uint8_t FieldB{};
    std::uint8_t FieldC{};
    std::uint8_t FieldD{};
    std::uint8_t FieldE{};
    std::uint8_t FieldF{};
    std::uint8_t Field10{};
    std::int32_t Field14{};
    std::int32_t Field18{};
    std::int32_t Field1C{};
    std::int32_t Field20{};
    std::int32_t Field24{};
    bool Field28{};
    std::int32_t Field2C{};
    bool Field30{};
    formats::Vector3 Field34{};
    std::int32_t Field40{};
    std::int32_t Field44{};
    AiPersonalityData1* Data1{};
    std::int32_t CallCount{};
    std::int32_t Depth{};
    std::vector<std::int32_t> Weights;
};

// PlayerHud.cs
struct LocatorInfo {
    formats::Vector3 Position{};
    ModelInstance* Model{};
    formats::ColorRgb Color{};
    float Alpha{};
};

// PlayerHud.cs
struct HudMessage {
    formats::Vector2 Position{};
    float FontSize{};
    formats::ColorRgba Color{};
    float Lifetime{};
    float Alpha{};
    std::uint8_t Category{};
    std::int32_t MaxWidth{};
    Align* Align{};
    std::vector<char> Text;
    bool DialogHide{};
};

// PlayerPause.cs
struct MapLegendInfo {
    bool Unlocked{};
    std::int32_t Group{};
    std::int32_t MessageId{};
    std::int32_t OffsetX{};
    std::int32_t OffsetY{};
    HudObjectInstance* HudObject{};
    std::int32_t ObjectIndex{};
};

// PlatformEntity.cs
struct SfxData {
    std::int32_t Id{};
    PlatSfxFlags Flags{};
};

// PlatformEntity.cs
struct MoveSfxInfo {
    SfxData Start1{};
    SfxData Start2{};
    SfxData Stop{};
    SfxData Destoryed{};
};

// ObjectEntity.cs
struct EffectSfxInfo {
    std::int32_t SfxId{};
    std::uint8_t Data{};
    bool Environment{};
};

// RoomEntity.cs
struct PortalNodeRef {
    Portal* Portal{};
    std::int32_t NodeIndex{};
};

// NodeData.cs
struct NodeData3 {
    NodeType NodeType{};
    std::uint16_t Id{};
    std::uint32_t Field4{};
    std::int32_t Count2{};
    formats::Vector3 Position{};
    float MaxDistance{};
    std::int32_t Index1{};
    std::int32_t Index2{};
    std::vector<std::uint16_t> Values;
    formats::Matrix4 Transform{};
    formats::Vector4 Color{};
    std::vector<formats::Vector4> _nodeDataColors;
};

// PlayerAi.cs
struct AiGlobals {
    PlayerEntity* Player{};
    std::int32_t Field4{};
    std::int32_t NodeDataIndex{};
    std::vector<NodeData3> NodeData;
};

// PlayerAi.cs
struct AiEntityRefs {
    NodeData3 Field0{};
    NodeData3 Field1{};
    NodeData3 Field2{};
    NodeData3 Field3{};
    NodeData3 Field4{};
    NodeData3 Field5{};
    NodeData3 Field6{};
    NodeData3 Field7{};
    NodeData3 Field8{};
    NodeData3 Field9{};
    NodeData3 Field10{};
    NodeData3 Field11{};
    NodeData3 Field12{};
    NodeData3 Field13{};
    NodeData3 Field14{};
    NodeData3 Field15{};
    NodeData3 Field16{};
    NodeData3 Field17{};
    NodeData3 Field18{};
    NodeData3 Field19{};
    NodeData3 Field20{};
    NodeData3 Field21{};
    NodeData3 Field22{};
    NodeData3 Field23{};
    NodeData3 Field24{};
    NodeData3 Field25{};
    PlayerEntity* Field26{};
    PlayerEntity* Field27{};
    PlayerEntity* Field28{};
    PlayerEntity* Field29{};
    PlayerEntity* Field30{};
    PlayerEntity* Field31{};
    PlayerEntity* Field32{};
    HalfturretEntity* Field33{};
    ItemSpawnEntity* Field34{};
    ItemSpawnEntity* Field35{};
    ItemSpawnEntity* Field36{};
    ItemSpawnEntity* Field37{};
    ItemSpawnEntity* Field38{};
    ItemSpawnEntity* Field39{};
    ItemSpawnEntity* Field40{};
    ItemSpawnEntity* Field41{};
    ItemSpawnEntity* Field42{};
    ItemSpawnEntity* Field43{};
    ItemSpawnEntity* Field44{};
    ItemSpawnEntity* Field45{};
    ItemSpawnEntity* Field46{};
    ItemSpawnEntity* Field47{};
    ItemSpawnEntity* Field48{};
    ItemSpawnEntity* Field49{};
    ItemSpawnEntity* Field50{};
    ItemSpawnEntity* Field51{};
    ItemSpawnEntity* Field52{};
    ItemSpawnEntity* Field53{};
    ItemInstanceEntity* Field54{};
    ItemInstanceEntity* Field55{};
    ItemInstanceEntity* Field56{};
    ItemInstanceEntity* Field57{};
    ItemInstanceEntity* Field58{};
    ItemInstanceEntity* Field59{};
    ItemInstanceEntity* Field60{};
    ItemInstanceEntity* Field61{};
    ItemInstanceEntity* Field62{};
    ItemInstanceEntity* Field63{};
    ItemInstanceEntity* Field64{};
    ItemInstanceEntity* Field65{};
    ItemInstanceEntity* Field66{};
    ItemInstanceEntity* Field67{};
    ItemInstanceEntity* Field68{};
    ItemInstanceEntity* Field69{};
    ItemInstanceEntity* Field70{};
    ItemInstanceEntity* Field71{};
    ItemInstanceEntity* Field72{};
    ItemInstanceEntity* Field73{};
    NodeDefenseEntity* Field74{};
    NodeDefenseEntity* Field75{};
    NodeDefenseEntity* Field76{};
    DoorEntity* Field77{};
};

} // namespace fruityprime::entities
