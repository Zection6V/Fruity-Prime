#pragma once

// Native counterpart of the effect runtime records in Formats/Effects.cs and
// the decoded effect tables in Formats/Formats.cs.
//
// An effect is a tree: an entry owns element entries, an element entry owns
// particles, and each level carries the function table that drives it.  The
// managed classes reference each other, so the records here hold pointers
// rather than nesting by value.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Formats/formats_layouts.hpp"
#include "Formats/model_format.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

class EntityCollision;
class Model;

struct EffectEntry;
struct EffectFuncBase;
struct EffectElementEntry;
struct EffectParticle;
struct Effect;
struct EffectElement;
struct Particle;

// Effects.cs
struct EffectEntry {
    std::int32_t EffectId{};
    std::vector<EffectElementEntry*> Elements;
};

// Effects.cs
// EffectFuncBase: the action and function tables an element is driven by.
struct EffectFuncBase {
    std::map<std::uint32_t, FxFuncInfo> Actions;
    std::map<std::uint32_t, FxFuncInfo> Funcs;
};

struct EffectElementEntry : EffectFuncBase {
    std::int32_t EffectId{};
    std::string_view EffectName;
    std::string_view ElementName;
    float CreationTime{};
    float ExpirationTime{};
    float DrainTime{};
    float BufferTime{};
    float Lifespan{};
    EffElemFlags Flags{};
    std::int32_t DrawType{};
    formats::Matrix4 OwnTransform{};
    formats::Matrix4 Transform{};
    formats::Vector3 Acceleration{};
    bool Func39Called{};
    float ParticleAmount{};
    bool Expired{};
    std::int32_t ChildEffectId{};
    float RoField1{};
    float RoField2{};
    float RoField3{};
    float RoField4{};
    std::int32_t Parity{};
    std::vector<Particle*> ParticleDefinitions;
    std::vector<std::int32_t> TextureBindingIds;
    std::vector<EffectParticle*> Particles;
    EffectElement* Definition{};
    EntityCollision* EntityCollision{};
    EffectEntry* EffectEntry{};
    Model* Model{};
    std::vector<model::Node> Nodes;
};

// Effects.cs
struct EffectParticle : EffectFuncBase {
    float CreationTime{};
    float ExpirationTime{};
    float Lifespan{};
    formats::Vector3 Position{};
    formats::Vector3 Speed{};
    float Scale{};
    float Rotation{};
    float Red{};
    float Green{};
    float Blue{};
    float Alpha{};
    std::int32_t ParticleId{};
    float PortionTotal{};
    float RoField1{};
    float RoField2{};
    float RoField3{};
    float RoField4{};
    float RwField1{};
    float RwField2{};
    float RwField3{};
    float RwField4{};
    EffectElementEntry* Owner{};
    std::int32_t MaterialId{};
    std::int32_t SetVecsId{};
    std::int32_t DrawId{};
    formats::Vector3 EffectVec1{};
    formats::Vector3 EffectVec2{};
    formats::Vector3 EffectVec3{};
    bool ShouldDraw{};
    formats::Vector3 Color{};
    formats::Vector2 Texcoord0{};
    formats::Vector3 Vertex0{};
    formats::Vector2 Texcoord1{};
    formats::Vector3 Vertex1{};
    formats::Vector2 Texcoord2{};
    formats::Vector3 Vertex2{};
    formats::Vector2 Texcoord3{};
    formats::Vector3 Vertex3{};
    bool DrawNode{};
    BillboardMode BillboardMode{};
    formats::Matrix4 NodeTransform{};
};

// Formats.cs
struct Effect {
    std::int32_t Id{};
    std::string_view Name;
    std::uint32_t Field0{};
    std::vector<std::uint32_t> List2;
    std::vector<EffectElement*> Elements;
    bool Persistent{};
};

// Formats.cs
struct EffectElement {
    std::string_view Name;
    std::string_view ModelName;
    std::vector<Particle*> Particles;
    EffElemFlags Flags{};
    formats::Vector3 Acceleration{};
    std::uint32_t ChildEffectId{};
    float Lifespan{};
    float DrainTime{};
    float BufferTime{};
    std::int32_t DrawType{};
};

// Formats.cs
struct Particle {
    std::string_view Name;
    Model* Model{};
    model::Node Node{};
    std::int32_t MaterialId{};
};

} // namespace fruityprime::formats
