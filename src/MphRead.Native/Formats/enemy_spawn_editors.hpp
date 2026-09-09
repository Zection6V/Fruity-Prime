#pragma once

// Native counterpart of the enemy-spawn editor records in
// Formats/EntityEnemy.cs: the mutable view of a spawner's union payload that
// the editor and the repacker write back.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Formats/formats_layouts.hpp"
#include "Formats/enemy_spawn_layouts.hpp"
#include "Formats/entity_editors.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

// Formats/EntityEnemy.cs
struct EnemySpawnEntityEditor : EntityEditorBase {
    EnemyType EnemyType{};
    std::int16_t LinkedEntityId{};
    std::uint8_t SpawnTotal{};
    std::uint8_t SpawnLimit{};
    std::uint8_t SpawnCount{};
    bool Active{};
    bool AlwaysActive{};
    std::uint8_t ItemChance{};
    std::uint16_t SpawnerHealth{};
    std::uint16_t CooldownTime{};
    std::uint16_t InitialCooldown{};
    float ActiveDistance{};
    float EnemyActiveDistance{};
    std::string_view SpawnNodeName;
    std::int16_t EntityId1{};
    Message Message1{};
    std::int16_t EntityId2{};
    Message Message2{};
    std::int16_t EntityId3{};
    Message Message3{};
    ItemType ItemType{};
    std::uint32_t EnemySubtype{};
    std::uint32_t EnemyVersion{};
    CollisionVolume Volume0{};
    CollisionVolume Volume1{};
    CollisionVolume Volume2{};
    CollisionVolume Volume3{};
    formats::Vector3 PathVector{};
    formats::Vector3 EnemyFacing{};
    formats::Vector3 EnemyPosition{};
    formats::Vector3 IdleRange{};
    std::uint32_t Unused68{};
    std::uint32_t Unused6C{};
    std::uint32_t Unused70{};
    std::uint32_t Unused74{};
    std::uint32_t Unused78{};
    std::uint32_t Unused7C{};
    std::uint32_t Unused80{};
    std::int32_t WeaveOffset{};
    std::int32_t Unknown01{};
    std::uint16_t EnemyHealth{};
    std::uint16_t EnemyDamage{};
    std::vector<formats::Vector3> MovementVectors;
    std::uint8_t PositionCount{};
    std::uint32_t MovementType{};
    Hunter Hunter{};
    std::uint32_t EncounterType{};
    std::uint32_t HunterWeapon{};
    std::uint16_t HunterHealth{};
    std::uint16_t HunterHealthMax{};
    std::uint16_t HunterHealthThreshold{};
    std::uint8_t HunterColor{};
    std::uint8_t HunterChance{};
    std::int32_t Index{};
    formats::Vector3 Unknown05{};
    float Unknown06{};
    float Unknown07{};
    std::int32_t SpawnerType{};
};

// Formats/EntityEnemy.cs
struct FhEnemySpawnEntityEditor : EntityEditorBase {
    CollisionVolume Box{};
    CollisionVolume Cylinder{};
    CollisionVolume Sphere{};
    FhEnemyType EnemyType{};
    std::uint8_t SpawnTotal{};
    std::uint8_t SpawnLimit{};
    std::uint8_t SpawnCount{};
    std::uint16_t Cooldown{};
    std::uint16_t StartFrame{};
    std::string_view SpawnNodeName;
    std::int16_t ParentId{};
    FhMessage EmptyMessage{};
};

} // namespace fruityprime::formats
